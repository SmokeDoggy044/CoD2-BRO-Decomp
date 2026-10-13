#pragma once

// Host input devices behind the XPP replacement (xinput.cpp), on SDL3.
//
// Port 0 is always connected: it merges the keyboard and mouse with the first PC
// gamepad, so the title always has a controller. Further gamepads take ports 1-3 in
// the order they are plugged in and keep their port until unplugged.
//
// Keyboard and mouse (until the port has real PC controls):
//   WASD left stick         arrow keys D-pad         IJKL right stick
//   Space A   Escape B   R X   F Y   Q white   E black
//   Return start   Tab back   left shift left stick click   V right stick click
//   right mouse button / Z left trigger      left mouse button / C right trigger
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

// Sets a port's rumble motors (0-65535; left is the low-frequency one).
void set_rumble(uint32_t port, uint16_t left, uint16_t right);

}  // namespace input
