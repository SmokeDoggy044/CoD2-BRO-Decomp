#include "output.h"

#define AL_ALEXT_PROTOTYPES
#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>

#include <chrono>

#include "log.h"

namespace audio {

class Output::Source {
public:
    explicit Source(Voice& voice) : voice(voice) {}
    Voice& voice;
    ALuint source = 0;
    ALuint buffer = 0;
    uint32_t channels = 0;  // of the current callback buffer
};

namespace {

ALenum al_format(uint32_t channels) {
    switch (channels) {
        case 1: return AL_FORMAT_MONO16;
        case 2: return AL_FORMAT_STEREO16;
        case 4: return AL_FORMAT_QUAD16;
        case 6: return AL_FORMAT_51CHN16;
        default: return 0;
    }
}

// Runs on OpenAL's mixer thread.
ALsizei AL_APIENTRY render_callback(void* user, void* data, ALsizei bytes) noexcept {
    auto& source = *static_cast<Output::Source*>(user);
    const size_t frame_bytes = size_t{source.channels} * sizeof(int16_t);
    const size_t produced = source.voice.render(static_cast<int16_t*>(data), static_cast<size_t>(bytes) / frame_bytes);
    return static_cast<ALsizei>(produced * frame_bytes);  // short: the source stops after this
}

}  // namespace

std::unique_ptr<Output> Output::open(const OutputConfig& config) {
    std::unique_ptr<Output> output(new Output());
    output->rate_ = config.rate;
    if (config.device == OutputConfig::Device::Default) output->device_ = alcOpenDevice(nullptr);
    if (output->device_) {
        output->context_ = alcCreateContext(output->device_, nullptr);
    } else {
        if (config.device == OutputConfig::Device::Default)
            LOG_WARN("audio", "no audio output device; sound plays into a silent loopback device");
        output->loopback_ = true;
        output->device_ = alcLoopbackOpenDeviceSOFT(nullptr);
        const ALCint attributes[] = {ALC_FORMAT_CHANNELS_SOFT, ALC_STEREO_SOFT,  ALC_FORMAT_TYPE_SOFT, ALC_SHORT_SOFT,
                                     ALC_FREQUENCY,           ALCint(config.rate), 0};
        if (output->device_) output->context_ = alcCreateContext(output->device_, attributes);
    }
    if (!output->context_ || !alcMakeContextCurrent(output->context_)) {
        LOG_ERROR("audio", "cannot create an OpenAL context");
        return nullptr;
    }
    if (!alIsExtensionPresent("AL_SOFT_callback_buffer") || !alIsExtensionPresent("AL_SOFT_direct_channels")) {
        LOG_ERROR("audio", "OpenAL Soft 1.22 or newer is needed (AL_SOFT_callback_buffer)");
        return nullptr;
    }
    // Distance attenuation follows the title's rolloff settings and is computed by the
    // caller; OpenAL only pans. No doppler for now.
    alDistanceModel(AL_NONE);
    alDopplerFactor(0.0f);
    const char* name = alcGetString(output->device_, ALC_ALL_DEVICES_SPECIFIER);
    LOG_INFO("audio", "OpenAL %s on %s%s", alGetString(AL_VERSION), output->loopback_ ? "loopback" : (name ? name : "?"),
             config.device == OutputConfig::Device::ManualLoopback ? " (manual)" : "");
    if (output->loopback_ && config.device != OutputConfig::Device::ManualLoopback)
        output->driver_ = std::thread([raw = output.get()] { raw->drive_loopback(); });
    return output;
}

Output::~Output() {
    stopping_ = true;
    if (driver_.joinable()) driver_.join();
    for (auto& source : sources_) {
        alDeleteSources(1, &source->source);
        alDeleteBuffers(1, &source->buffer);
    }
    alcMakeContextCurrent(nullptr);
    if (context_) alcDestroyContext(context_);
    if (device_) alcCloseDevice(device_);
}

void Output::drive_loopback() {
    using namespace std::chrono;
    constexpr size_t kChunk = 480;  // 10 ms at 48 kHz
    std::vector<int16_t> scratch(kChunk * 2);
    auto next = steady_clock::now();
    while (!stopping_) {
        if (!paused_) alcRenderSamplesSOFT(device_, scratch.data(), static_cast<ALCsizei>(kChunk));
        next += microseconds(kChunk * 1000000 / rate_);
        std::this_thread::sleep_until(next);
    }
}

void Output::set_paused(bool paused) {
    if (paused_.exchange(paused) == paused || loopback_) return;
    if (!alcIsExtensionPresent(device_, "ALC_SOFT_pause_device")) return;
    if (paused)
        alcDevicePauseSOFT(device_);
    else
        alcDeviceResumeSOFT(device_);
}

void Output::render(int16_t* out, size_t frames) {
    if (loopback_) alcRenderSamplesSOFT(device_, out, static_cast<ALCsizei>(frames));
}

Output::Source* Output::add_source(Voice& voice) {
    auto source = std::make_unique<Source>(voice);
    alGenSources(1, &source->source);
    alGenBuffers(1, &source->buffer);
    Source* raw = source.get();
    sources_.push_back(std::move(source));
    reformat(raw);
    return raw;
}

void Output::reformat(Source* source) {
    const Format format = source->voice.format();
    alSourceStop(source->source);
    alSourcei(source->source, AL_BUFFER, 0);
    source->channels = 0;
    const ALenum al = al_format(format.channels);
    if (!al || format.block_align == 0) {
        if (format.block_align) LOG_WARN("audio", "%u-channel voices are not supported", format.channels);
        return;
    }
    source->channels = format.channels;
    alBufferCallbackSOFT(source->buffer, al, static_cast<ALsizei>(format.rate), render_callback, source);
    alSourcei(source->source, AL_BUFFER, static_cast<ALint>(source->buffer));
    if (alGetError() != AL_NO_ERROR) LOG_WARN("audio", "cannot set up a callback buffer");
}

void Output::update(Source* source, const SourceParams& params) {
    const ALuint s = source->source;
    alSourcef(s, AL_GAIN, params.gain);
    alSourcef(s, AL_PITCH, params.pitch);
    alSourcei(s, AL_DIRECT_CHANNELS_SOFT, params.placement == Placement::Direct ? AL_TRUE : AL_FALSE);
    alSourcei(s, AL_SOURCE_RELATIVE, params.placement == Placement::World ? AL_FALSE : AL_TRUE);
    alSourcefv(s, AL_POSITION, params.position);
}

void Output::sync(Source* source) {
    if (!source->channels) return;
    ALint state = 0;
    alGetSourcei(source->source, AL_SOURCE_STATE, &state);
    const bool playing = source->voice.playing();
    const bool paused = source->voice.paused();
    if (playing && !paused && state != AL_PLAYING)
        alSourcePlay(source->source);
    else if (playing && paused && state == AL_PLAYING)
        alSourcePause(source->source);
    else if (!playing && (state == AL_PLAYING || state == AL_PAUSED))
        alSourceStop(source->source);
}

void Output::stop(Source* source) { alSourceStop(source->source); }

void Output::set_listener(const float position[3], const float velocity[3], const float at[3], const float up[3]) {
    const float orientation[6] = {at[0], at[1], at[2], up[0], up[1], up[2]};
    alListenerfv(AL_POSITION, position);
    alListenerfv(AL_VELOCITY, velocity);
    alListenerfv(AL_ORIENTATION, orientation);
}

void Output::begin_batch() { alDeferUpdatesSOFT(); }
void Output::end_batch() { alProcessUpdatesSOFT(); }

bool Output::set_hrtf(bool enabled) {
    if (loopback_) return false;
    const ALCint attributes[] = {ALC_HRTF_SOFT, enabled ? ALC_TRUE : ALC_FALSE, 0};
    return alcResetDeviceSOFT(device_, attributes) == ALC_TRUE;
}

}  // namespace audio
