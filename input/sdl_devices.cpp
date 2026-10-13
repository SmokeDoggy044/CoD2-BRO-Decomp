// Host input devices on SDL3. See devices.h.

#include "devices.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <iterator>
#include <utility>
#include <mutex>

#include "float_env.h"
#include "log.h"

namespace input {

namespace {

std::mutex g_mutex;  // the port table
bool g_started = false;
SDL_Gamepad* g_ports[kPortCount] = {};  // gamepad on each port (port 0 may have none)

// Opens new gamepads into free ports and closes unplugged ones. Needs g_mutex.
void refresh_ports() {
    for (auto& pad : g_ports) {
        if (pad && !SDL_GamepadConnected(pad)) {
            LOG_INFO("input", "gamepad removed");
            SDL_CloseGamepad(pad);
            pad = nullptr;
        }
    }
    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    if (!ids) return;
    for (int i = 0; i < count; ++i) {
        const bool open = std::any_of(std::begin(g_ports), std::end(g_ports),
                                      [&](SDL_Gamepad* pad) { return pad && SDL_GetGamepadID(pad) == ids[i]; });
        if (open) continue;
        auto* slot = std::find(std::begin(g_ports), std::end(g_ports), nullptr);
        if (slot == std::end(g_ports)) break;
        *slot = SDL_OpenGamepad(ids[i]);
        if (!*slot) {
            LOG_WARN("input", "cannot open gamepad: %s", SDL_GetError());
            continue;
        }
        const char* name = SDL_GetGamepadName(*slot);
        LOG_INFO("input", "gamepad on port %d: %s", static_cast<int>(slot - g_ports), name ? name : "?");
    }
    SDL_free(ids);
}

uint8_t analog(bool pressed) { return pressed ? 0xFF : 0; }

// Adds `value` to an axis, saturating.
void add_axis(int16_t& axis, int value) {
    axis = static_cast<int16_t>(std::clamp(axis + value, -32768, 32767));
}

void read_gamepad(SDL_Gamepad* pad, PadState& state) {
    const auto button = [&](SDL_GamepadButton b) { return SDL_GetGamepadButton(pad, b); };
    const std::pair<SDL_GamepadButton, uint16_t> digital[] = {
        {SDL_GAMEPAD_BUTTON_DPAD_UP, kDpadUp},       {SDL_GAMEPAD_BUTTON_DPAD_DOWN, kDpadDown},
        {SDL_GAMEPAD_BUTTON_DPAD_LEFT, kDpadLeft},   {SDL_GAMEPAD_BUTTON_DPAD_RIGHT, kDpadRight},
        {SDL_GAMEPAD_BUTTON_START, kStart},          {SDL_GAMEPAD_BUTTON_BACK, kBack},
        {SDL_GAMEPAD_BUTTON_LEFT_STICK, kLeftThumb}, {SDL_GAMEPAD_BUTTON_RIGHT_STICK, kRightThumb},
    };
    for (auto [b, bit] : digital)
        if (button(b)) state.buttons |= bit;
    const std::pair<SDL_GamepadButton, AnalogButton> buttons[] = {
        {SDL_GAMEPAD_BUTTON_SOUTH, kA},           {SDL_GAMEPAD_BUTTON_EAST, kB},
        {SDL_GAMEPAD_BUTTON_WEST, kX},            {SDL_GAMEPAD_BUTTON_NORTH, kY},
        {SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, kBlack}, {SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, kWhite},
    };
    for (auto [b, index] : buttons)
        state.analog_buttons[index] = std::max(state.analog_buttons[index], analog(button(b)));
    // Triggers: 0..32767 -> 0..255.
    const auto trigger = [&](SDL_GamepadAxis axis) {
        return static_cast<uint8_t>(std::clamp<int>(SDL_GetGamepadAxis(pad, axis), 0, 32767) >> 7);
    };
    state.analog_buttons[kLeftTrigger] =
        std::max(state.analog_buttons[kLeftTrigger], trigger(SDL_GAMEPAD_AXIS_LEFT_TRIGGER));
    state.analog_buttons[kRightTrigger] =
        std::max(state.analog_buttons[kRightTrigger], trigger(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER));
    // Sticks: SDL's down is positive, the Xbox's up is.
    add_axis(state.thumb_lx, SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX));
    add_axis(state.thumb_ly, -SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTY) - 1);
    add_axis(state.thumb_rx, SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHTX));
    add_axis(state.thumb_ry, -SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHTY) - 1);
}

void read_keyboard_mouse(PadState& state) {
    int key_count = 0;
    const bool* keys = SDL_GetKeyboardState(&key_count);
    const auto key = [&](SDL_Scancode code) { return code < key_count && keys[code]; };
    const auto stick = [&](SDL_Scancode minus, SDL_Scancode plus) { return (key(plus) - key(minus)) * 32767; };
    add_axis(state.thumb_lx, stick(SDL_SCANCODE_A, SDL_SCANCODE_D));
    add_axis(state.thumb_ly, stick(SDL_SCANCODE_S, SDL_SCANCODE_W));
    add_axis(state.thumb_rx, stick(SDL_SCANCODE_J, SDL_SCANCODE_L));
    add_axis(state.thumb_ry, stick(SDL_SCANCODE_K, SDL_SCANCODE_I));

    const std::pair<SDL_Scancode, uint16_t> digital[] = {
        {SDL_SCANCODE_UP, kDpadUp},         {SDL_SCANCODE_DOWN, kDpadDown},
        {SDL_SCANCODE_LEFT, kDpadLeft},     {SDL_SCANCODE_RIGHT, kDpadRight},
        {SDL_SCANCODE_RETURN, kStart},      {SDL_SCANCODE_TAB, kBack},
        {SDL_SCANCODE_LSHIFT, kLeftThumb},  {SDL_SCANCODE_V, kRightThumb},
    };
    for (auto [code, bit] : digital)
        if (key(code)) state.buttons |= bit;

    const SDL_MouseButtonFlags mouse = SDL_GetMouseState(nullptr, nullptr);
    const std::pair<bool, AnalogButton> buttons[] = {
        {key(SDL_SCANCODE_SPACE), kA},  {key(SDL_SCANCODE_ESCAPE), kB},
        {key(SDL_SCANCODE_R), kX},      {key(SDL_SCANCODE_F), kY},
        {key(SDL_SCANCODE_E), kBlack},  {key(SDL_SCANCODE_Q), kWhite},
        {key(SDL_SCANCODE_Z) || (mouse & SDL_BUTTON_RMASK), kLeftTrigger},
        {key(SDL_SCANCODE_C) || (mouse & SDL_BUTTON_LMASK), kRightTrigger},
    };
    for (auto [pressed, index] : buttons)
        state.analog_buttons[index] = std::max(state.analog_buttons[index], analog(pressed));
}

}  // namespace

bool init_devices() {
    platform::HostFloatEnv host_env;
    std::lock_guard lock(g_mutex);
    if (g_started) return true;
    if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD)) {
        LOG_WARN("input", "SDL gamepads: %s", SDL_GetError());
        return false;
    }
    g_started = true;
    refresh_ports();
    return true;
}

uint32_t connected_ports() {
    platform::HostFloatEnv host_env;
    std::lock_guard lock(g_mutex);
    uint32_t mask = 1;  // port 0: keyboard and mouse
    if (!g_started) return mask;
    refresh_ports();
    for (uint32_t port = 1; port < kPortCount; ++port)
        if (g_ports[port]) mask |= 1u << port;
    return mask;
}

bool read_port(uint32_t port, PadState& state) {
    platform::HostFloatEnv host_env;
    state = PadState{};
    if (port >= kPortCount) return false;
    std::lock_guard lock(g_mutex);
    if (g_ports[port]) read_gamepad(g_ports[port], state);
    if (port == 0) {
        read_keyboard_mouse(state);
        return true;
    }
    return g_ports[port] != nullptr;
}

void set_rumble(uint32_t port, uint16_t left, uint16_t right) {
    platform::HostFloatEnv host_env;
    if (port >= kPortCount) return;
    std::lock_guard lock(g_mutex);
    // The Xbox keeps a rumble going until the title changes it; SDL wants a duration.
    // Titles refresh it often enough that a few seconds is plenty.
    if (g_ports[port]) SDL_RumbleGamepad(g_ports[port], left, right, 5000);
}

}  // namespace input
