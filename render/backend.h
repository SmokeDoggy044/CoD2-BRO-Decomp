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

// One draw: float4 attributes and a primitive. The back end reads everything else
// (shaders, textures, render states) from the Xbox-side state (nv2a.h,
// shader_state.h, d3d_internal.h) and translates it.
struct DrawInput {
    enum class Primitive { Invalid, Points, Lines, LineStrip, Triangles, TriangleStrip, TriangleFan };
    Primitive primitive = Primitive::Invalid;
    const float* vertices = nullptr;  // per vertex: one float4 per attribute in attribute_mask, in order
    uint32_t vertex_count = 0;
    uint32_t attribute_mask = 0;        // vertex shader inputs v0..v15 present
    const uint32_t* indices = nullptr;  // null: vertices in order
    uint32_t index_count = 0;
    float depth_max = 16777215.0f;  // the depth buffer's largest value (screen-space z range)
};

class Backend {
public:
    virtual ~Backend() = default;

    enum ClearFlags : uint32_t { kClearColor = 1, kClearDepth = 2, kClearStencil = 4 };
    // Clears the whole current render target / depth buffer. color is A8R8G8B8.
    virtual void clear(uint32_t flags, uint32_t color, float depth, uint32_t stencil) = 0;

    // Replaces the back buffer's pixels with CPU-written ones (X8R8G8B8, back buffer
    // size, `pitch` bytes per row).
    virtual void write_back_buffer(const void* pixels, uint32_t pitch) = 0;

    // Saves the back buffer as a 24-bit BMP. Returns false (logged) on failure.
    virtual bool save_back_buffer(const char* path) = 0;

    // Shows the back buffer.
    virtual void present() = 0;

    // Draws to the back buffer with the current Xbox state.
    virtual void draw(const DrawInput& input) = 0;

    // Handles window events. Returns false once the user closed the window.
    virtual bool pump_events() = 0;
};

// Null if the window or device cannot be created (logged).
std::unique_ptr<Backend> create_d3d9_backend(const BackendConfig& config);

}  // namespace render
