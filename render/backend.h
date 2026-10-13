#pragma once

// The renderer's back end: a window and a PC graphics API. The Xbox D3D front end
// (device.cpp) turns the title's calls into these; D3D9 (DXVK on Linux) is the first
// implementation, others (Vulkan) can follow behind the same interface.

#include <cstdint>
#include <memory>

namespace render {

struct BackendConfig {
    uint32_t width = 640;  // the title's back buffer size
    uint32_t height = 480;
    uint32_t window_scale = 1;
    bool vsync = true;
};

class Backend {
public:
    virtual ~Backend() = default;

    enum ClearFlags : uint32_t { kClearColor = 1, kClearDepth = 2, kClearStencil = 4 };
    // Clears the whole current render target / depth buffer. color is A8R8G8B8.
    virtual void clear(uint32_t flags, uint32_t color, float depth, uint32_t stencil) = 0;

    // Shows the back buffer.
    virtual void present() = 0;

    // Handles window events. Returns false once the user closed the window.
    virtual bool pump_events() = 0;
};

// Null if the window or device cannot be created (logged).
std::unique_ptr<Backend> create_d3d9_backend(const BackendConfig& config);

}  // namespace render
