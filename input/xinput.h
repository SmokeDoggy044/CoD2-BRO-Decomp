#pragma once

// Xbox input (XPP library: XInputOpen & co, device enumeration) replacement.
//
// For now one gamepad is always connected on port 0 and reports neutral input, so
// titles boot; SDL3 gamepads and the keyboard come later behind the same functions.
// No memory units, no voice communicator.

#include <span>

#include "kernel.h"
#include "types.h"

namespace input {

struct InputConfig {
    // The title's XPP device type tables (XDEVICE_TYPE_GAMEPAD is &g_DeviceType_Gamepad).
    xbox::XbPtr gamepad_type = 0;
};

void configure(const InputConfig& config);

// The replacements, by SDK function name. The loader redirects each function it finds.
std::span<const xbox::KernelImpl> xpp_functions();

}  // namespace input
