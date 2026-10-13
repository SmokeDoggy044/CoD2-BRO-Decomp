// D3D9 draw translation. See d3d9_draw.h.

#include "d3d9_draw.h"

#include <algorithm>
#include <bit>
#include <cstring>
#include <mutex>
#include <set>
#include <unordered_map>
#include <vector>

#include "d3d9_shader.h"
#include "d3d_internal.h"
#include "log.h"
#include "nv2a.h"
#include "shader_state.h"
#include "texture_decode.h"
#include "xbox_d3d.h"

namespace render {

namespace {

using xbox::from_xbox_ptr;

void warn_once(std::once_flag& once, const char* message) {
    std::call_once(once, [message] { LOG_WARN("render", "%s", message); });
}

uint64_t hash_bytes(const void* data, size_t size, uint64_t seed = 0xCBF29CE484222325ull) {
    const auto* p = static_cast<const uint8_t*>(data);
    uint64_t h = seed ^ (size * 0x9E3779B97F4A7C15ull);
    for (; size >= 8; size -= 8, p += 8) {
        uint64_t v;
        std::memcpy(&v, p, 8);
        h = (h ^ v) * 0x100000001B3ull;
        h ^= h >> 29;
    }
    for (; size; --size, ++p) h = (h ^ *p) * 0x100000001B3ull;
    return h;
}

template <typename T>
void release(T*& object) {
    if (object) object->Release();
    object = nullptr;
}

// --- State translation (NV2A values are OpenGL enums) ------------------------------------

D3DCMPFUNC compare_func(uint32_t gl) {
    return gl >= 0x200 && gl <= 0x207 ? static_cast<D3DCMPFUNC>(gl - 0x200 + 1) : D3DCMP_ALWAYS;
}

D3DBLEND blend_factor(uint32_t gl) {
    switch (gl) {
        case 0x0000: return D3DBLEND_ZERO;
        case 0x0001: return D3DBLEND_ONE;
        case 0x0300: return D3DBLEND_SRCCOLOR;
        case 0x0301: return D3DBLEND_INVSRCCOLOR;
        case 0x0302: return D3DBLEND_SRCALPHA;
        case 0x0303: return D3DBLEND_INVSRCALPHA;
        case 0x0304: return D3DBLEND_DESTALPHA;
        case 0x0305: return D3DBLEND_INVDESTALPHA;
        case 0x0306: return D3DBLEND_DESTCOLOR;
        case 0x0307: return D3DBLEND_INVDESTCOLOR;
        case 0x0308: return D3DBLEND_SRCALPHASAT;
        case 0x8001: case 0x8003: return D3DBLEND_BLENDFACTOR;  // constant colour / alpha
        case 0x8002: case 0x8004: return D3DBLEND_INVBLENDFACTOR;
        default: return D3DBLEND_ONE;
    }
}

D3DBLENDOP blend_op(uint32_t gl) {
    switch (gl) {
        case 0x800A: return D3DBLENDOP_SUBTRACT;
        case 0x800B: case 0xF005: return D3DBLENDOP_REVSUBTRACT;  // 0xF005: Xbox signed variant
        case 0x8007: return D3DBLENDOP_MIN;
        case 0x8008: return D3DBLENDOP_MAX;
        default: return D3DBLENDOP_ADD;
    }
}

D3DSTENCILOP stencil_op(uint32_t gl) {
    switch (gl) {
        case 0x0000: return D3DSTENCILOP_ZERO;
        case 0x1E01: return D3DSTENCILOP_REPLACE;
        case 0x1E02: return D3DSTENCILOP_INCRSAT;
        case 0x1E03: return D3DSTENCILOP_DECRSAT;
        case 0x150A: return D3DSTENCILOP_INVERT;
        case 0x8507: return D3DSTENCILOP_INCR;
        case 0x8508: return D3DSTENCILOP_DECR;
        default: return D3DSTENCILOP_KEEP;
    }
}

D3DCULL cull_mode(uint32_t cull, uint32_t front_face) {
    if (cull == kCullNone) return D3DCULL_NONE;
    const bool cull_cw = (cull == kCullCw) != (front_face == kFrontFaceCcw);
    return cull_cw ? D3DCULL_CW : D3DCULL_CCW;
}

D3DFILLMODE fill_mode(uint32_t fill) {
    switch (fill) {
        case kFillPoint: return D3DFILL_POINT;
        case kFillWireframe: return D3DFILL_WIREFRAME;
        default: return D3DFILL_SOLID;
    }
}

DWORD texture_address(uint32_t xbox) {
    if (xbox >= 1 && xbox <= 4) return xbox;  // wrap, mirror, clamp, border
    return xbox == 5 ? DWORD{D3DTADDRESS_CLAMP} : DWORD{D3DTADDRESS_WRAP};  // 5: clamp to edge
}

DWORD texture_filter(uint32_t xbox, bool allow_none) {
    switch (xbox) {
        case 0: return allow_none ? D3DTEXF_NONE : D3DTEXF_POINT;
        case 1: return D3DTEXF_POINT;
        case 3: return D3DTEXF_ANISOTROPIC;
        default: return D3DTEXF_LINEAR;  // linear, quincunx, gaussian
    }
}

void unpack_color(uint32_t color, float* out) {
    out[0] = ((color >> 16) & 0xFF) / 255.0f;
    out[1] = ((color >> 8) & 0xFF) / 255.0f;
    out[2] = (color & 0xFF) / 255.0f;
    out[3] = (color >> 24) / 255.0f;
}

// The register combiner program, from the NV2A registers.
XbPixelShaderDef combiners_from_registers() {
    XbPixelShaderDef d{};
    for (uint32_t i = 0; i < 8; ++i) {
        d.alpha_inputs[i] = nv2a::reg(kNvCombinerAlphaInputs + i * 4);
        d.constant0[i] = nv2a::reg(kNvCombinerFactor0 + i * 4);
        d.constant1[i] = nv2a::reg(kNvCombinerFactor1 + i * 4);
        d.alpha_outputs[i] = nv2a::reg(kNvCombinerAlphaOutputs + i * 4);
        d.rgb_inputs[i] = nv2a::reg(kNvCombinerColorInputs + i * 4);
        d.rgb_outputs[i] = nv2a::reg(kNvCombinerColorOutputs + i * 4);
    }
    d.final_inputs_abcd = nv2a::reg(kNvCombinerFinal0);
    d.final_inputs_efg = nv2a::reg(kNvCombinerFinal1);
    d.final_constant0 = nv2a::reg(kNvCombinerFinalFactor);
    d.final_constant1 = nv2a::reg(kNvCombinerFinalFactor + 4);
    d.combiner_count = nv2a::reg(kNvCombinerControl);
    d.texture_modes = nv2a::reg(kNvShaderStageProgram);
    d.dot_mapping = nv2a::reg(kNvDotRgbMapping);
    d.input_texture = nv2a::reg(kNvShaderOtherStageInput);
    return d;
}

bool stage_has_texture(uint32_t stage) { return nv2a::reg(kNvTextureFormat + stage * kNvTextureStride) != 0; }

// No pixel shader: D3D's texture stages. Approximated as stage 0's default
// (texture * diffuse, or diffuse alone) until the stage states are translated.
XbPixelShaderDef fixed_function_combiners() {
    static std::once_flag once;
    warn_once(once, "fixed-function texture stages: approximated as texture0 * diffuse");
    XbPixelShaderDef d{};
    d.combiner_count = 1;
    if (stage_has_texture(0)) {
        d.rgb_inputs[0] = 0x08040000;    // T0 * V0
        d.alpha_inputs[0] = 0x18140000;  // T0.a * V0.a
        d.texture_modes = d3d9::kModeProject2D;
    } else {
        d.rgb_inputs[0] = 0x04200000;    // V0 * 1
        d.alpha_inputs[0] = 0x14200000;  // V0.a * 1
    }
    d.rgb_outputs[0] = 0xC0;  // AB -> R0
    d.alpha_outputs[0] = 0xC0;
    return d;
}

uint32_t primitive_count(DrawInput::Primitive primitive, uint32_t n) {
    switch (primitive) {
        case DrawInput::Primitive::Points: return n;
        case DrawInput::Primitive::Lines: return n / 2;
        case DrawInput::Primitive::LineStrip: return n > 1 ? n - 1 : 0;
        case DrawInput::Primitive::Triangles: return n / 3;
        case DrawInput::Primitive::TriangleStrip:
        case DrawInput::Primitive::TriangleFan: return n > 2 ? n - 2 : 0;
        default: return 0;
    }
}

D3DPRIMITIVETYPE primitive_type(DrawInput::Primitive primitive) {
    switch (primitive) {
        case DrawInput::Primitive::Points: return D3DPT_POINTLIST;
        case DrawInput::Primitive::Lines: return D3DPT_LINELIST;
        case DrawInput::Primitive::LineStrip: return D3DPT_LINESTRIP;
        case DrawInput::Primitive::TriangleStrip: return D3DPT_TRIANGLESTRIP;
        case DrawInput::Primitive::TriangleFan: return D3DPT_TRIANGLEFAN;
        default: return D3DPT_TRIANGLELIST;
    }
}

}  // namespace

struct D3D9Draw::CachedTexture {
    IDirect3DTexture9* texture = nullptr;
    uint64_t content = 0;
    uint32_t checked_frame = 0;
    float scale[2] = {1.0f, 1.0f};  // texel -> normalized coordinates for linear formats
};

struct D3D9Draw::Caches {
    std::unordered_map<uint32_t, IDirect3DVertexDeclaration9*> declarations;
    std::unordered_map<uint64_t, IDirect3DVertexShader9*> vertex_shaders;
    std::unordered_map<uint64_t, IDirect3DPixelShader9*> pixel_shaders;
    std::unordered_map<uint64_t, D3D9Draw::CachedTexture> textures;
    std::set<uint32_t> unsupported_formats;  // warned about once each

    ~Caches() {
        for (auto& [k, v] : declarations) release(v);
        for (auto& [k, v] : vertex_shaders) release(v);
        for (auto& [k, v] : pixel_shaders) release(v);
        for (auto& [k, v] : textures) release(v.texture);
    }
};

D3D9Draw::D3D9Draw(IDirect3DDevice9* device, uint32_t width, uint32_t height)
    : device_(device), width_(width), height_(height), caches_(std::make_unique<Caches>()) {
    device_->SetRenderState(D3DRS_LIGHTING, FALSE);
    device_->SetRenderState(D3DRS_FOGENABLE, FALSE);
    device_->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
}

D3D9Draw::~D3D9Draw() = default;

void D3D9Draw::end_frame() { ++frame_; }

D3D9Draw::CachedTexture* D3D9Draw::texture_for_stage(uint32_t stage) {
    const uint32_t data = nv2a::reg(kNvTextureOffset + stage * kNvTextureStride);
    const uint32_t format = nv2a::reg(kNvTextureFormat + stage * kNvTextureStride);
    if (!format) return nullptr;
    // Linear formats keep their size only in the D3D object.
    const TextureStage& bound = texture_stage(stage);
    uint32_t size_field = 0;
    if (bound.texture) {
        const auto* pixels = from_xbox_ptr<XbPixelContainer>(bound.texture);
        if (pixels->resource.data == data) size_field = pixels->size;
    }
    TextureLayout layout;
    if (!describe_texture(format, size_field, layout)) {
        if (caches_->unsupported_formats.insert(format).second)
            LOG_WARN("render", "texture format register %08x not supported yet (stage %u)", format, stage);
        return nullptr;
    }
    const uint32_t* palette = nullptr;
    uint32_t palette_data = 0;
    if (layout.color_format == 0x0B && bound.palette) {  // P8
        palette_data = from_xbox_ptr<XbResource>(bound.palette)->data;
        palette = from_xbox_ptr<uint32_t>(palette_data | 0x80000000u);
    }

    const uint32_t key_words[4] = {data, format, size_field, palette_data};
    CachedTexture& entry = caches_->textures[hash_bytes(key_words, sizeof(key_words))];
    if (entry.texture && entry.checked_frame == frame_) return &entry;
    entry.checked_frame = frame_;
    const auto* memory = from_xbox_ptr<uint8_t>(data | 0x80000000u);
    uint64_t content = hash_bytes(memory, layout.total_size);
    if (palette) content = hash_bytes(palette, 256 * 4, content);
    if (entry.texture && entry.content == content) return &entry;
    entry.content = content;

    if (!entry.texture) {
        D3DFORMAT d3d_format = D3DFMT_A8R8G8B8;
        switch (layout.kind) {
            case TextureLayout::Kind::Dxt1: d3d_format = D3DFMT_DXT1; break;
            case TextureLayout::Kind::Dxt3: d3d_format = D3DFMT_DXT3; break;
            case TextureLayout::Kind::Dxt5: d3d_format = D3DFMT_DXT5; break;
            default: break;
        }
        if (FAILED(device_->CreateTexture(layout.width, layout.height, layout.levels, 0, d3d_format, D3DPOOL_MANAGED,
                                          &entry.texture, nullptr))) {
            LOG_ERROR("render", "CreateTexture %ux%u format %08x failed", layout.width, layout.height, format);
            entry.texture = nullptr;
            return nullptr;
        }
        if (layout.linear) {
            entry.scale[0] = 1.0f / layout.width;
            entry.scale[1] = 1.0f / layout.height;
        }
    }
    for (uint32_t level = 0; level < layout.levels; ++level) {
        D3DLOCKED_RECT locked;
        if (FAILED(entry.texture->LockRect(level, &locked, nullptr, 0))) continue;
        decode_level(layout, level, memory, palette, static_cast<uint8_t*>(locked.pBits),
                     static_cast<uint32_t>(locked.Pitch));
        entry.texture->UnlockRect(level);
    }
    return &entry;
}

namespace {

IDirect3DVertexDeclaration9* vertex_declaration(IDirect3DDevice9* device,
                                                std::unordered_map<uint32_t, IDirect3DVertexDeclaration9*>& cache,
                                                uint32_t mask) {
    auto& entry = cache[mask];
    if (entry) return entry;
    std::vector<D3DVERTEXELEMENT9> elements;
    WORD offset = 0;
    for (uint32_t i = 0; i < kMaxAttributes; ++i) {
        if (!(mask & (1u << i))) continue;
        elements.push_back({0, offset, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, static_cast<BYTE>(i)});
        offset += 16;
    }
    elements.push_back(D3DDECL_END());
    if (FAILED(device->CreateVertexDeclaration(elements.data(), &entry))) LOG_ERROR("render", "vertex declaration failed");
    return entry;
}

void log_tokens(const char* what, const std::vector<uint32_t>& tokens) {
    std::string text;
    for (size_t i = 0; i < tokens.size() && i < 256; ++i) {
        char buf[12];
        std::snprintf(buf, sizeof buf, "%08x ", tokens[i]);
        text += buf;
    }
    LOG_ERROR("render", "%s failed; tokens: %s", what, text.c_str());
}

}  // namespace

void D3D9Draw::draw(const DrawInput& input) {
    Caches& caches = *caches_;
    const uint32_t vertex_count_or_indices = input.indices ? input.index_count : input.vertex_count;
    const uint32_t primitives = primitive_count(input.primitive, vertex_count_or_indices);
    if (!primitives) return;

    // Vertex shader: the program from its start slot to the final instruction.
    const nv2a::State& nv = nv2a::state();
    const uint32_t start = std::min(nv2a::reg(kNvProgramStart), nv2a::kProgramSlots - 1);
    uint32_t count = 0;
    while (start + count < nv2a::kProgramSlots) {
        ++count;
        if (nv.program[start + count - 1][3] & 1) break;
    }
    const uint64_t vs_key = hash_bytes(nv.program[start].data(), count * 16, input.attribute_mask);
    IDirect3DVertexShader9*& vs = caches.vertex_shaders[vs_key];
    if (!vs) {
        const auto tokens = d3d9::translate_vertex_program(
            reinterpret_cast<const uint32_t(*)[4]>(nv.program[start].data()), count, input.attribute_mask);
        if (FAILED(device_->CreateVertexShader(reinterpret_cast<const DWORD*>(tokens.data()), &vs))) {
            log_tokens("CreateVertexShader", tokens);
            vs = nullptr;
            caches.vertex_shaders.erase(vs_key);
            return;
        }
    }

    // Pixel shader: the combiners without their constants (those are shader constants).
    d3d9::PixelShaderKey ps_input{};
    ps_input.def = shader_state().pixel_shader_set ? combiners_from_registers() : fixed_function_combiners();
    const uint32_t* tss = texture_state();
    for (uint32_t stage = 0; stage < kTextureStages; ++stage)
        if (tss[stage * kTssPerStage + kTssAlphaKill]) ps_input.alpha_kill_mask |= 1u << stage;
    d3d9::PixelShaderKey ps_key = ps_input;
    std::memset(ps_key.def.constant0, 0, sizeof(ps_key.def.constant0));
    std::memset(ps_key.def.constant1, 0, sizeof(ps_key.def.constant1));
    ps_key.def.final_constant0 = ps_key.def.final_constant1 = 0;
    const uint64_t ps_hash = hash_bytes(&ps_key, sizeof(ps_key));
    IDirect3DPixelShader9*& ps = caches.pixel_shaders[ps_hash];
    if (!ps) {
        const auto tokens = d3d9::translate_pixel_shader(ps_key);
        if (FAILED(device_->CreatePixelShader(reinterpret_cast<const DWORD*>(tokens.data()), &ps))) {
            log_tokens("CreatePixelShader", tokens);
            ps = nullptr;
            caches.pixel_shaders.erase(ps_hash);
            return;
        }
    }

    // Vertex constants: the NV2A slots, then screen space -> clip space.
    device_->SetVertexShaderConstantF(0, reinterpret_cast<const float*>(nv.constants.data()), nv2a::kConstantSlots);
    const float screen_to_clip[8] = {2.0f / width_, -2.0f / height_, 1.0f / input.depth_max, 0.0f,
                                     -1.0f, 1.0f, 0.0f, 0.0f};
    device_->SetVertexShaderConstantF(d3d9::kVsScreenToClip, screen_to_clip, 2);

    // Textures and samplers; linear textures scale their texel coordinates.
    float ps_constants[(d3d9::kPsTexcoordScale + 4) * 4] = {};
    for (uint32_t stage = 0; stage < kTextureStages; ++stage) {
        float* scale = ps_constants + (d3d9::kPsTexcoordScale + stage) * 4;
        scale[0] = scale[1] = scale[2] = scale[3] = 1.0f;
        const uint32_t mode = d3d9::texture_mode(ps_input.def, stage);
        IDirect3DBaseTexture9* texture = nullptr;
        if (mode == d3d9::kModeProject2D) {
            if (CachedTexture* cached = texture_for_stage(stage)) {
                texture = cached->texture;
                scale[0] = cached->scale[0];
                scale[1] = cached->scale[1];
            }
        } else if (mode == d3d9::kModeCubemap || mode == d3d9::kModeProject3D) {
            static std::once_flag once;
            warn_once(once, "cube and volume textures: not supported yet");
        }
        device_->SetTexture(stage, texture);
        if (!texture) continue;
        const uint32_t* s = tss + stage * kTssPerStage;
        device_->SetSamplerState(stage, D3DSAMP_ADDRESSU, texture_address(s[kTssAddressU]));
        device_->SetSamplerState(stage, D3DSAMP_ADDRESSV, texture_address(s[kTssAddressV]));
        device_->SetSamplerState(stage, D3DSAMP_ADDRESSW, texture_address(s[kTssAddressW]));
        device_->SetSamplerState(stage, D3DSAMP_MAGFILTER, texture_filter(s[kTssMagFilter], false));
        device_->SetSamplerState(stage, D3DSAMP_MINFILTER, texture_filter(s[kTssMinFilter], false));
        device_->SetSamplerState(stage, D3DSAMP_MIPFILTER, texture_filter(s[kTssMipFilter], true));
        device_->SetSamplerState(stage, D3DSAMP_MIPMAPLODBIAS, s[kTssMipMapLodBias]);
        device_->SetSamplerState(stage, D3DSAMP_MAXMIPLEVEL, s[kTssMaxMipLevel]);
        device_->SetSamplerState(stage, D3DSAMP_MAXANISOTROPY, std::max<uint32_t>(1, s[kTssMaxAnisotropy]));
        device_->SetSamplerState(stage, D3DSAMP_BORDERCOLOR, s[kTssBorderColor]);
    }

    // Pixel constants: combiner factors per stage, final factors, texcoord scales.
    for (uint32_t i = 0; i < 8; ++i) {
        unpack_color(ps_input.def.constant0[i], ps_constants + (d3d9::kPsStageC0 + i) * 4);
        unpack_color(ps_input.def.constant1[i], ps_constants + (d3d9::kPsStageC1 + i) * 4);
    }
    unpack_color(ps_input.def.final_constant0, ps_constants + d3d9::kPsFinalC0 * 4);
    unpack_color(ps_input.def.final_constant1, ps_constants + d3d9::kPsFinalC1 * 4);
    device_->SetPixelShaderConstantF(0, ps_constants, d3d9::kPsTexcoordScale + 4);

    // Raster state.
    const uint32_t* rs = render_state();
    auto reg = [](uint32_t method) { return nv2a::reg(method); };
    device_->SetRenderState(D3DRS_ZENABLE, rs[kRsZEnable] ? D3DZB_TRUE : D3DZB_FALSE);
    device_->SetRenderState(D3DRS_ZWRITEENABLE, reg(kNvDepthMask) != 0);
    device_->SetRenderState(D3DRS_ZFUNC, compare_func(reg(kNvDepthFunc)));
    device_->SetRenderState(D3DRS_ALPHATESTENABLE, reg(kNvAlphaTestEnable) != 0);
    device_->SetRenderState(D3DRS_ALPHAFUNC, compare_func(reg(kNvAlphaFunc)));
    device_->SetRenderState(D3DRS_ALPHAREF, reg(kNvAlphaRef) & 0xFF);
    device_->SetRenderState(D3DRS_ALPHABLENDENABLE, reg(kNvBlendEnable) != 0);
    device_->SetRenderState(D3DRS_SRCBLEND, blend_factor(reg(kNvBlendSrc)));
    device_->SetRenderState(D3DRS_DESTBLEND, blend_factor(reg(kNvBlendDst)));
    device_->SetRenderState(D3DRS_BLENDOP, blend_op(reg(kNvBlendEquation)));
    device_->SetRenderState(D3DRS_BLENDFACTOR, reg(kNvBlendColor));
    const uint32_t mask = reg(kNvColorMask);
    device_->SetRenderState(D3DRS_COLORWRITEENABLE, (mask & 0x00010000 ? D3DCOLORWRITEENABLE_RED : 0) |
                                                        (mask & 0x00000100 ? D3DCOLORWRITEENABLE_GREEN : 0) |
                                                        (mask & 0x00000001 ? D3DCOLORWRITEENABLE_BLUE : 0) |
                                                        (mask & 0x01000000 ? D3DCOLORWRITEENABLE_ALPHA : 0));
    device_->SetRenderState(D3DRS_CULLMODE, cull_mode(rs[kRsCullMode], rs[kRsFrontFace]));
    device_->SetRenderState(D3DRS_FILLMODE, fill_mode(rs[kRsFillMode]));
    device_->SetRenderState(D3DRS_STENCILENABLE, rs[kRsStencilEnable] != 0);
    device_->SetRenderState(D3DRS_STENCILFAIL, stencil_op(rs[kRsStencilFail]));
    device_->SetRenderState(D3DRS_STENCILZFAIL, stencil_op(reg(kNvStencilOpZFail)));
    device_->SetRenderState(D3DRS_STENCILPASS, stencil_op(reg(kNvStencilOpZPass)));
    device_->SetRenderState(D3DRS_STENCILFUNC, compare_func(reg(kNvStencilFunc)));
    device_->SetRenderState(D3DRS_STENCILREF, reg(kNvStencilFuncRef));
    device_->SetRenderState(D3DRS_STENCILMASK, reg(kNvStencilFuncMask));
    device_->SetRenderState(D3DRS_STENCILWRITEMASK, reg(kNvStencilMask));
    device_->SetRenderState(D3DRS_SHADEMODE, reg(kNvShadeMode) == 0x1D00 ? D3DSHADE_FLAT : D3DSHADE_GOURAUD);

    // The whole target is the D3D9 viewport (the vertex program did the Xbox one);
    // the Xbox viewport rectangle clips.
    D3DVIEWPORT9 viewport{0, 0, width_, height_, 0.0f, 1.0f};
    device_->SetViewport(&viewport);
    const XbViewport& v = view_state().viewport;
    RECT clip{static_cast<LONG>(std::min(v.x, width_)), static_cast<LONG>(std::min(v.y, height_)),
              static_cast<LONG>(std::min(v.x + v.width, width_)), static_cast<LONG>(std::min(v.y + v.height, height_))};
    device_->SetScissorRect(&clip);

    device_->SetVertexDeclaration(vertex_declaration(device_, caches.declarations, input.attribute_mask));
    device_->SetVertexShader(vs);
    device_->SetPixelShader(ps);
    const UINT stride = static_cast<UINT>(std::popcount(input.attribute_mask)) * 16;
    if (input.indices)
        device_->DrawIndexedPrimitiveUP(primitive_type(input.primitive), 0, input.vertex_count, primitives,
                                        input.indices, D3DFMT_INDEX32, input.vertices, stride);
    else
        device_->DrawPrimitiveUP(primitive_type(input.primitive), primitives, input.vertices, stride);
}

}  // namespace render
