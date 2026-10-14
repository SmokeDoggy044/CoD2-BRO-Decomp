#pragma once

// The D3D9 back end's draw translation: Xbox vertex programs, register combiners,
// textures and raster state -> D3D9 objects and states, cached across draws.

#include <cstdint>
#include <memory>

#include "backend.h"
#include "d3d9_api.h"
#include "shader_state.h"

namespace render {

class D3D9Draw {
public:
    // `config`: the back buffer and the device's depth buffer (the D3D9 device's own).
    D3D9Draw(IDirect3DDevice9* device, const BackendConfig& config);
    ~D3D9Draw();
    D3D9Draw(const D3D9Draw&) = delete;
    D3D9Draw& operator=(const D3D9Draw&) = delete;

    void draw(const DrawInput& input);
    void end_frame();  // textures are checked for CPU changes once per frame

    // Render targets: D3D9 render target textures by Xbox memory address. False if the
    // target cannot be created (draws then go nowhere).
    bool set_render_target(const SurfaceDesc& color, const SurfaceDesc* depth);
    void forget_render_target(uint32_t data);
    // The surface drawn into for `data` (null if none yet).
    IDirect3DSurface9* target_surface(uint32_t data);
    bool has_depth() const { return has_depth_; }

private:
    struct Caches;
    struct CachedTexture;
    struct Target;
    // The texture bound to a stage as a D3D9 texture (created or updated), or null.
    CachedTexture* texture_for_stage(uint32_t stage);
    void dump_draw(const DrawInput& input, const XbPixelShaderDef& def, uint64_t vs_key, uint32_t primitives);

    IDirect3DDevice9* device_;
    uint32_t depth_data_;  // the device's depth buffer's Xbox address
    uint32_t target_data_ = 0, target_width_ = 0, target_height_ = 0;
    bool has_depth_ = false;
    bool target_ok_ = false;
    uint32_t frame_ = 1;
    uint32_t dump_frame_ = 0;  // XB_DUMP_FRAME / XB_DUMP_STOP, see dump_draw
    uint32_t dump_stop_ = UINT32_MAX;
    uint32_t dump_index_ = 0;
    std::unique_ptr<Caches> caches_;
};

}  // namespace render
