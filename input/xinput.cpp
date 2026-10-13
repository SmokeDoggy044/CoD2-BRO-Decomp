// Xbox input replacement. See xinput.h.

#include "xinput.h"

#include <atomic>
#include <cstring>

#include "compiler.h"
#include "dispatcher.h"
#include "log.h"
#include "objects.h"

namespace input {

namespace {

using xbox::from_xbox_ptr;
using xbox::XbPtr;

constexpr uint32_t kErrorSuccess = 0;
constexpr uint32_t kErrorIoPending = 997;
constexpr uint32_t kErrorDeviceNotConnected = 1167;
constexpr uint32_t kConnectedPorts = 0x1;  // port 0
constexpr uint32_t kHandleBase = 0x58494E00;  // 'XIN' + port: opaque handles

InputConfig g_config;
std::atomic<uint32_t> g_packet{0};

bool is_gamepad(XbPtr type) { return type != 0 && type == g_config.gamepad_type; }

// XINPUT_GAMEPAD
struct XbGamepad {
    uint16_t buttons;
    uint8_t analog_buttons[8];
    int16_t thumb_lx, thumb_ly, thumb_rx, thumb_ry;
};
static_assert(sizeof(XbGamepad) == 18);

#pragma pack(push, 1)
struct XbInputState {  // XINPUT_STATE
    uint32_t packet_number;
    XbGamepad gamepad;
};
#pragma pack(pop)
static_assert(sizeof(XbInputState) == 22);

struct XbFeedbackHeader {  // XINPUT_FEEDBACK_HEADER
    uint32_t status;
    XbPtr event;
};

// --- Devices ----------------------------------------------------------------------------

void XB_STDCALL XInitDevices(uint32_t /*prealloc_count*/, XbPtr /*prealloc_types*/) {}

// Bitmask of connected ports.
uint32_t XB_STDCALL XGetDevices(XbPtr type) { return is_gamepad(type) ? kConnectedPorts : 0; }

// Nothing is ever plugged in or out after XGetDevices.
uint32_t XB_STDCALL XGetDeviceChanges(XbPtr /*type*/, XbPtr insertions, XbPtr removals) {
    if (insertions) *from_xbox_ptr<uint32_t>(insertions) = 0;
    if (removals) *from_xbox_ptr<uint32_t>(removals) = 0;
    return 0;
}

// Like XGetDeviceChanges without consuming the changes.
uint32_t XB_STDCALL XPeekDevices(XbPtr type, XbPtr insertions, XbPtr removals) {
    return XGetDeviceChanges(type, insertions, removals);
}

// --- Gamepads ---------------------------------------------------------------------------

XbPtr XB_STDCALL XInputOpen(XbPtr type, uint32_t port, uint32_t /*slot*/, XbPtr /*polling*/) {
    if (!is_gamepad(type) || !(kConnectedPorts & (1u << port))) return 0;
    return kHandleBase + port;
}

void XB_STDCALL XInputClose(XbPtr /*handle*/) {}

// XINPUT_CAPABILITIES: subtype, then the controls the device has (all of them).
uint32_t XB_STDCALL XInputGetCapabilities(XbPtr /*handle*/, XbPtr capabilities) {
    auto* bytes = from_xbox_ptr<uint8_t>(capabilities);
    bytes[0] = 1;  // XINPUT_DEVSUBTYPE_GC_GAMEPAD
    return kErrorSuccess;
}

uint32_t XB_STDCALL XInputGetState(XbPtr /*handle*/, XbPtr state) {
    auto* s = from_xbox_ptr<XbInputState>(state);
    s->packet_number = ++g_packet;
    s->gamepad = XbGamepad{};
    return kErrorSuccess;
}

// Rumble: accepted and completed at once.
uint32_t XB_STDCALL XInputSetState(XbPtr /*handle*/, XbPtr feedback) {
    auto* header = from_xbox_ptr<XbFeedbackHeader>(feedback);
    header->status = kErrorSuccess;
    if (header->event) xbox::set_event_handle(header->event);
    return kErrorIoPending;
}

// --- Memory units: none ---------------------------------------------------------------

uint32_t XB_STDCALL XMountMUA(uint32_t /*port*/, uint32_t /*slot*/, XbPtr /*drive*/) { return kErrorDeviceNotConnected; }
uint32_t XB_STDCALL XUnmountMU(uint32_t /*port*/, uint32_t /*slot*/) { return kErrorDeviceNotConnected; }

#define XPP(name) {#name, reinterpret_cast<void*>(&name)}

const xbox::KernelImpl kFunctions[] = {
    XPP(XGetDeviceChanges),
    XPP(XGetDevices),
    XPP(XInitDevices),
    XPP(XInputClose),
    XPP(XInputGetCapabilities),
    XPP(XInputGetState),
    XPP(XInputOpen),
    XPP(XInputSetState),
    XPP(XMountMUA),
    XPP(XPeekDevices),
    XPP(XUnmountMU),
};

}  // namespace

void configure(const InputConfig& config) { g_config = config; }

std::span<const xbox::KernelImpl> xpp_functions() { return kFunctions; }

}  // namespace input
