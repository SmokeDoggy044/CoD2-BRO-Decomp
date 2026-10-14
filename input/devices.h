#pragma once

// Host input devices behind the XPP replacement (xinput.cpp), on SDL3.
//
// Port 0 is always connected: it merges the keyboard and mouse with the first PC
// gamepad, so the title always has a controller. Further gamepads take ports 1-3 in
// the order they are plugged in and keep their port until unplugged.
//
// Keyboard and mouse, on the pad buttons of CoD BRO's default layout (config/
// default.cfg); the action in brackets:
//   WASD left stick (move)          IJKL right stick (look; the mouse looks directly,
//                                   see take_mouse_motion)
//   left mouse  right trigger (fire)     right mouse  left trigger (aim down sights)
//   Space  Y (jump)    C / left Ctrl  B (crouch, prone)    R  X (reload)
//   F  A (use)         G  white (grenade)    V / middle mouse  black (melee)
//   left Shift  left stick click (sprint)    X  right stick click
//   mouse wheel / 1  D-pad up (next weapon)  Q / E  D-pad left / right (lean)
//   Escape  start (pause)    Tab  back (objectives)
//   menus: arrow keys D-pad, Return A (select), Backspace B (back)
// Clicking into the window captures the mouse; focus loss (Alt+Tab) or F10 frees it.
// Gamepads use their Xbox-style layout; LB is white, RB is black.
//
// Every function may be called from any thread. SDL updates the devices while the
// renderer pumps window events.

#include <cstdint>

namespace input {

constexpr uint32_t kPortCount = 4;

// An Xbox gamepad's controls, in XINPUT_GAMEPAD units.
struct PadState {
    uint16_t buttons = 0;           // XINPUT_GAMEPAD_* digital buttons
    uint8_t analog_buttons[8] = {};  // A B X Y black white left-trigger right-trigger
    int16_t thumb_lx = 0, thumb_ly = 0, thumb_rx = 0, thumb_ry = 0;  // up is positive
};

enum DigitalButton : uint16_t {
    kDpadUp = 0x01, kDpadDown = 0x02, kDpadLeft = 0x04, kDpadRight = 0x08,
    kStart = 0x10, kBack = 0x20, kLeftThumb = 0x40, kRightThumb = 0x80,
};
enum AnalogButton { kA, kB, kX, kY, kBlack, kWhite, kLeftTrigger, kRightTrigger };

// Starts SDL's gamepad support. False (logged) if it is unavailable; port 0 then
// still exists and reports the keyboard, if a window gets created.
bool init_devices();

// Bitmask of connected ports (bit n = port n). Picks up plugged/unplugged gamepads.
uint32_t connected_ports();

// The current state of a port. False if nothing is connected there.
bool read_port(uint32_t port, PadState& state);

// Mouse motion (counts) since the last call while the mouse is captured; 0 otherwise.
// Titles take it where they turn the view (a title-specific hook).
void take_mouse_motion(float& dx, float& dy);

// Sets a port's rumble motors (0-65535; left is the low-frequency one).
void set_rumble(uint32_t port, uint16_t left, uint16_t right);

}  // namespace input
