#pragma once

// Audio output through OpenAL Soft. Each voice is an OpenAL source whose samples come
// from Voice::render through a callback buffer (AL_SOFT_callback_buffer), so the Xbox
// playback rules stay in Voice and OpenAL does resampling, 3D panning and HRTF.
//
// Without a usable output device (or when asked), a loopback device is used instead:
// it renders into nothing at real-time pace, so voices still advance and stream packets
// still complete. Tests drive the loopback device themselves (render()).

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "voice.h"

typedef struct ALCdevice ALCdevice;
typedef struct ALCcontext ALCcontext;

namespace audio {

struct OutputConfig {
    enum class Device { Default, Loopback, ManualLoopback };
    Device device = Device::Default;  // ManualLoopback: nothing renders unless render() is called
    uint32_t rate = 48000;            // loopback rate
};

class Output {
public:
    enum class Placement { Direct, Relative, World };
    struct SourceParams {
        float gain = 1.0f;
        float pitch = 1.0f;
        Placement placement = Placement::Direct;  // Direct: channels straight to speakers
        float position[3] = {0, 0, 0};            // OpenAL coordinates (right-handed)
    };
    class Source;

    static std::unique_ptr<Output> open(const OutputConfig& config);
    ~Output();

    // Sources are created for a voice and follow its format (call reformat() after
    // changing it, with the voice stopped).
    Source* add_source(Voice& voice);
    void reformat(Source* source);
    void update(Source* source, const SourceParams& params);
    // Starts, pauses or stops the OpenAL source to match the voice's state.
    void sync(Source* source);
    void stop(Source* source);

    void set_listener(const float position[3], const float velocity[3], const float at[3], const float up[3]);
    // Between begin_batch() and end_batch() changes apply together.
    void begin_batch();
    void end_batch();
    bool set_hrtf(bool enabled);

    bool is_loopback() const { return loopback_; }
    // Loopback only: mixes `frames` stereo 16-bit frames.
    void render(int16_t* out, size_t frames);

private:
    Output() = default;
    void drive_loopback();

    ALCdevice* device_ = nullptr;
    ALCcontext* context_ = nullptr;
    bool loopback_ = false;
    uint32_t rate_ = 48000;
    std::vector<std::unique_ptr<Source>> sources_;
    std::thread driver_;
    std::atomic<bool> stopping_{false};
};

}  // namespace audio
