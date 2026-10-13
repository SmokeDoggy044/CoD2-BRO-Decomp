#pragma once

// The D3D9 back end's draw translation: Xbox vertex programs, register combiners,
// textures and raster state -> D3D9 objects and states, cached across draws.

#include <cstdint>
#include <memory>

#include "backend.h"
#include "d3d9_api.h"

namespace render {

class D3D9Draw {
public:
    D3D9Draw(IDirect3DDevice9* device, uint32_t width, uint32_t height);
    ~D3D9Draw();
    D3D9Draw(const D3D9Draw&) = delete;
    D3D9Draw& operator=(const D3D9Draw&) = delete;

    void draw(const DrawInput& input);
    void end_frame();  // textures are checked for CPU changes once per frame

private:
    struct Caches;
    struct CachedTexture;
    // The texture bound to a stage as a D3D9 texture (created or updated), or null.
    CachedTexture* texture_for_stage(uint32_t stage);

    IDirect3DDevice9* device_;
    uint32_t width_, height_;
    uint32_t frame_ = 1;
    std::unique_ptr<Caches> caches_;
};

}  // namespace render
