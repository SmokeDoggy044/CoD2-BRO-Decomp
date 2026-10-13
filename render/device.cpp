// Xbox Direct3D 8 replacement: device, frame and synchronization functions. See d3d.h.
//
// Semantics follow the XDK and what CoD2: BRO relies on:
// - Everything the title submits is executed (or dropped) synchronously, so fences are
//   always passed and InsertCallback runs its callback at once.
// - The vertical blank callback runs on our own 60 Hz thread at DISPATCH_LEVEL, like
//   the Xbox's DPC. Titles pace their frames with it (CoD2: BRO busy-waits on a flag
//   that its callback clears), so it must keep running.

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "compiler.h"
#include "d3d.h"
#include "d3d_internal.h"
#include "dispatcher.h"
#include "float_env.h"
#include "log.h"
#include "xbox_d3d.h"

namespace render {

namespace {

using xbox::from_xbox_ptr;
using xbox::to_xbox_ptr;

constexpr uint32_t kVBlankHz = 60;  // NTSC

struct State {
    D3DConfig config;
    std::unique_ptr<Backend> backend;
    uint8_t* device = nullptr;  // our CDevice block (title-addressable)
    XbSurface* back_buffer = nullptr;
    XbSurface* depth_stencil = nullptr;
    uint32_t fence = 0;
    uint32_t swap_count = 0;
    XbVBlankData* vblank_data = nullptr;
    std::atomic<uint32_t> vblank_count{0};
    std::atomic<uint32_t> vblank_callback{0};
};
State& g = *new State;  // the vertical blank thread uses it until exit: never destroyed

template <typename T>
T& device_field(uint32_t offset) {
    return *reinterpret_cast<T*>(g.device + offset);
}

// A D3D-created surface (render target or depth buffer). Its pixels live in the
// back end, so Data stays 0.
XbSurface* create_surface(uint32_t format, uint32_t width, uint32_t height, uint32_t bytes_per_pixel) {
    auto* surface = new XbSurface{};
    surface->container.resource.common = kCommonTypeSurface | kCommonD3DCreated | 1;
    surface->container.format = kFormatDmaChannelA | (2u << kFormatDimensionShift) |
                                (format << kFormatFormatShift) | (1u << kFormatMipmapShift);
    const uint32_t pitch = (width * bytes_per_pixel + 63) & ~63u;
    surface->container.size =
        (width - 1) | ((height - 1) << kSizeHeightShift) | ((pitch / 64 - 1) << kSizePitchShift);
    return surface;
}

XbSurface* add_ref(XbSurface* surface) {
    if (surface) ++surface->container.resource.common;  // the count is the low 16 bits
    return surface;
}

void call_title(uint32_t function, std::initializer_list<uint32_t> args) {
    if (auto call_guest = xbox::kernel_config().call_guest)
        call_guest(function, std::data(args), static_cast<uint32_t>(args.size()));
    else
        LOG_ERROR("render", "cannot call title function %08x", function);
}

void vblank_thread() {
    if (auto attach = xbox::kernel_config().attach_kernel_thread) attach();
    using Clock = std::chrono::steady_clock;
    const auto period = std::chrono::nanoseconds(1'000'000'000 / kVBlankHz);
    auto next = Clock::now();
    for (;;) {
        next += period;
        std::this_thread::sleep_until(next);
        const uint32_t count = ++g.vblank_count;
        const uint32_t callback = g.vblank_callback.load();
        if (!callback) continue;
        g.vblank_data->vblank = count;
        g.vblank_data->swap = g.swap_count;
        g.vblank_data->flags = 0;
        const uint8_t old_irql = xbox::raise_irql_to(xbox::kDispatchLevel);
        call_title(callback, {to_xbox_ptr(g.vblank_data)});  // cdecl; call_guest restores ESP
        xbox::lower_irql_to(old_irql);
    }
}

// --- Device ---------------------------------------------------------------------------

uint32_t XB_STDCALL Direct3D_CreateDevice(uint32_t /*adapter*/, uint32_t /*device_type*/, XbPtr /*window*/,
                                          uint32_t /*behavior_flags*/, XbPtr parameters, XbPtr out_device) {
    const auto* pp = from_xbox_ptr<XbPresentParameters>(parameters);
    LOG_INFO("render", "CreateDevice %ux%u format %02x x%u, multisample %x, depth %u/%02x, flags %x, interval %x",
             pp->back_buffer_width, pp->back_buffer_height, pp->back_buffer_format, pp->back_buffer_count,
             pp->multisample_type, pp->enable_auto_depth_stencil, pp->auto_depth_stencil_format, pp->flags,
             pp->presentation_interval);
    if (g.device) {
        LOG_ERROR("render", "CreateDevice called twice");
        return kD3DErrInvalidCall;
    }

    BackendConfig backend_config = g.config.backend;
    backend_config.width = pp->back_buffer_width ? pp->back_buffer_width : 640;
    backend_config.height = pp->back_buffer_height ? pp->back_buffer_height : 480;
    platform::HostFloatEnv float_env;
    g.backend = create_d3d9_backend(backend_config);
    if (!g.backend) return kD3DErrInvalidCall;

    g.device = new uint8_t[kDeviceSize]{};
    g.back_buffer = create_surface(kFmtLinX8R8G8B8, backend_config.width, backend_config.height, 4);
    if (pp->enable_auto_depth_stencil)
        g.depth_stencil = create_surface(pp->auto_depth_stencil_format, backend_config.width, backend_config.height,
                                         pp->auto_depth_stencil_format == kFmtD16 ? 2 : 4);
    device_field<XbPtr>(kDeviceBackBuffer) = to_xbox_ptr(g.back_buffer);
    device_field<XbPtr>(kDeviceRenderTarget) = to_xbox_ptr(g.back_buffer);
    device_field<XbPtr>(kDeviceDepthStencil) = to_xbox_ptr(g.depth_stencil);
    g.vblank_data = new XbVBlankData{};

    if (g.config.device_pointer) *from_xbox_ptr<XbPtr>(g.config.device_pointer) = to_xbox_ptr(g.device);
    *from_xbox_ptr<XbPtr>(out_device) = to_xbox_ptr(g.device);
    std::thread(vblank_thread).detach();
    return kD3DOk;
}

void XB_STDCALL D3D_SetPushBufferSize(uint32_t /*size*/, uint32_t /*kickoff_size*/) {}

void XB_STDCALL D3DDevice_GetDeviceCaps(XbPtr caps) {
    // D3DCAPS8 (0xD4 bytes). CoD2: BRO never reads it.
    std::memset(from_xbox_ptr<void>(caps), 0, 0xD4);
}

void XB_STDCALL D3DDevice_SetGammaRamp(uint32_t /*flags*/, XbPtr /*ramp*/) {
    static std::once_flag once;
    std::call_once(once, [] { LOG_WARN("render", "SetGammaRamp ignored"); });
}

// --- Surfaces -------------------------------------------------------------------------

// index -1 = the front buffer, 0 = the back buffer.
XbPtr XB_STDCALL D3DDevice_GetBackBuffer2(int32_t /*index*/) { return to_xbox_ptr(add_ref(g.back_buffer)); }

XbPtr XB_STDCALL D3DDevice_GetRenderTarget2() {
    return to_xbox_ptr(add_ref(from_xbox_ptr<XbSurface>(device_field<XbPtr>(kDeviceRenderTarget))));
}

XbPtr XB_STDCALL D3DDevice_GetDepthStencilSurface2() {
    return to_xbox_ptr(add_ref(from_xbox_ptr<XbSurface>(device_field<XbPtr>(kDeviceDepthStencil))));
}

uint32_t XB_STDCALL D3DDevice_SetRenderTarget(XbPtr target, XbPtr depth_stencil) {
    // null target = keep the current one (XDK behavior).
    if (target) device_field<XbPtr>(kDeviceRenderTarget) = target;
    device_field<XbPtr>(kDeviceDepthStencil) = depth_stencil;
    if ((target && target != to_xbox_ptr(g.back_buffer)) ||
        (depth_stencil && depth_stencil != to_xbox_ptr(g.depth_stencil))) {
        static std::once_flag once;
        std::call_once(once, [] { LOG_WARN("render", "render targets other than the back buffer: not yet drawn"); });
    }
    return kD3DOk;
}

// --- Frame ----------------------------------------------------------------------------

uint32_t XB_STDCALL D3DDevice_Clear(uint32_t rect_count, XbPtr /*rects*/, uint32_t flags, uint32_t color, float z,
                                    uint32_t stencil) {
    if (rect_count) {
        static std::once_flag once;
        std::call_once(once, [] { LOG_WARN("render", "Clear with rectangles: clearing everything"); });
    }
    if (device_field<XbPtr>(kDeviceRenderTarget) != to_xbox_ptr(g.back_buffer)) return kD3DOk;
    uint32_t backend_flags = 0;
    if (flags & kClearTargetMask) backend_flags |= Backend::kClearColor;
    if (flags & kClearZBuffer) backend_flags |= Backend::kClearDepth;
    if (flags & kClearStencil) backend_flags |= Backend::kClearStencil;
    platform::HostFloatEnv float_env;
    g.backend->clear(backend_flags, color, z, stencil);
    return kD3DOk;
}

uint32_t XB_STDCALL D3DDevice_Swap(uint32_t /*flags*/) {
    platform::HostFloatEnv float_env;
    g.backend->present();
    if (!g.backend->pump_events()) {
        LOG_INFO("render", "window closed");
        std::exit(0);
    }
    device_field<uint32_t>(kDeviceSwapCount) = ++g.swap_count;
    return g.swap_count;
}

void XB_STDCALL D3DDevice_SetVerticalBlankCallback(XbPtr callback) { g.vblank_callback = callback; }

void XB_STDCALL D3DDevice_GetDisplayFieldStatus(XbPtr status) {
    auto* s = from_xbox_ptr<XbFieldStatus>(status);
    s->field = 3;  // progressive
    s->vblank_count = g.vblank_count;
}

// --- Synchronization --------------------------------------------------------------------

uint32_t XB_STDCALL D3DDevice_InsertFence() { return ++g.fence; }

void XB_STDCALL D3DDevice_BlockOnFence(uint32_t /*fence*/) {}

// type 0 = read callback, 1 = write callback; either runs once the GPU gets there,
// which for us is now. The callback is cdecl: void (DWORD context).
void XB_STDCALL D3DDevice_InsertCallback(uint32_t /*type*/, XbPtr callback, uint32_t context) {
    if (callback) call_title(callback, {context});
}

// --- The table -------------------------------------------------------------------------

}  // namespace

const xbox::KernelImpl kDeviceFunctions[] = {
    D3D_IMPL(D3D_SetPushBufferSize),
    D3D_IMPL(D3DDevice_BlockOnFence),
    D3D_IMPL(D3DDevice_Clear),
    D3D_IMPL(D3DDevice_GetBackBuffer2),
    D3D_IMPL(D3DDevice_GetDepthStencilSurface2),
    D3D_IMPL(D3DDevice_GetDeviceCaps),
    D3D_IMPL(D3DDevice_GetDisplayFieldStatus),
    D3D_IMPL(D3DDevice_GetRenderTarget2),
    D3D_IMPL(D3DDevice_InsertCallback),
    D3D_IMPL(D3DDevice_InsertFence),
    D3D_IMPL(D3DDevice_SetGammaRamp),
    D3D_IMPL(D3DDevice_SetRenderTarget),
    D3D_IMPL(D3DDevice_SetVerticalBlankCallback),
    D3D_IMPL(D3DDevice_Swap),
    D3D_IMPL(Direct3D_CreateDevice),
};
const size_t kDeviceFunctionCount = std::size(kDeviceFunctions);

void resource_destroyed(XbPtr /*resource*/) {}

void configure(const D3DConfig& config) { g.config = config; }

std::span<const xbox::KernelImpl> d3d_functions() {
    static const std::vector<xbox::KernelImpl> all = [] {
        std::vector<xbox::KernelImpl> v(kDeviceFunctions, kDeviceFunctions + kDeviceFunctionCount);
        v.insert(v.end(), kResourceFunctions, kResourceFunctions + kResourceFunctionCount);
        return v;
    }();
    return all;
}

}  // namespace render
