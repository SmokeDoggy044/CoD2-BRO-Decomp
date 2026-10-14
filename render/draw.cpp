// The draw path's front end. See draw.h.

#include "draw.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <mutex>
#include <vector>

#include "backend.h"
#include "compiler.h"
#include "d3d_internal.h"
#include "float_env.h"
#include "log.h"
#include "nv2a.h"
#include "shader_state.h"
#include "xbox_d3d.h"

namespace render {

namespace {

using xbox::from_xbox_ptr;

std::atomic<uint32_t> g_skipped_draws{0};  // see skipped_draws()

void warn_once(std::once_flag& once, const char* message) {
    std::call_once(once, [message] { LOG_WARN("render", "%s", message); });
}

// D3DVSDT_*: high nibble = component count, low nibble = type.
enum AttributeType : uint32_t {
    kTypeColor = 0x0, kTypeNormShort = 0x1, kTypeFloat = 0x2, kTypePackedByte = 0x4,
    kTypeShort = 0x5, kTypeNormPacked = 0x6,
};
constexpr uint32_t kFormatNone = 0x02;
constexpr uint32_t kFormatFloat2H = 0x72;

// Bytes one attribute takes in a vertex (0 = unknown format).
uint32_t attribute_size(uint32_t format) {
    if (format == kFormatFloat2H) return 12;
    const uint32_t count = format >> 4;
    switch (format & 0xF) {
        case kTypeColor: return 4;
        case kTypeNormShort:
        case kTypeShort: return 2 * count;
        case kTypeFloat: return 4 * count;
        case kTypePackedByte: return count;
        case kTypeNormPacked: return 4;
        default: return 0;
    }
}

int32_t sign_extend(uint32_t value, uint32_t bits) {
    const uint32_t shift = 32 - bits;
    return static_cast<int32_t>(value << shift) >> shift;
}

// One attribute -> float4; missing components default to (0, 0, 0, 1).
void fetch_attribute(uint32_t format, const uint8_t* p, float* out) {
    out[0] = out[1] = out[2] = 0.0f;
    out[3] = 1.0f;
    if (format == kFormatFloat2H) {  // (x, y, 0, w)
        std::memcpy(out, p, 8);
        std::memcpy(out + 3, p + 8, 4);
        return;
    }
    const uint32_t count = std::min<uint32_t>(format >> 4, 4);
    switch (format & 0xF) {
        case kTypeColor:  // D3DCOLOR: B, G, R, A in memory
            out[0] = p[2] / 255.0f;
            out[1] = p[1] / 255.0f;
            out[2] = p[0] / 255.0f;
            out[3] = p[3] / 255.0f;
            break;
        case kTypeNormShort:
        case kTypeShort:
            for (uint32_t i = 0; i < count; ++i) {
                int16_t v;
                std::memcpy(&v, p + i * 2, 2);
                out[i] = (format & 0xF) == kTypeShort ? v : std::max(v / 32767.0f, -1.0f);
            }
            break;
        case kTypeFloat: std::memcpy(out, p, 4 * count); break;
        case kTypePackedByte:
            for (uint32_t i = 0; i < count; ++i) out[i] = p[i] / 255.0f;
            break;
        case kTypeNormPacked: {  // 11:11:10 signed normalized
            uint32_t v;
            std::memcpy(&v, p, 4);
            out[0] = std::max(sign_extend(v & 0x7FF, 11) / 1023.0f, -1.0f);
            out[1] = std::max(sign_extend((v >> 11) & 0x7FF, 11) / 1023.0f, -1.0f);
            out[2] = std::max(sign_extend(v >> 22, 10) / 511.0f, -1.0f);
            break;
        }
        default: break;
    }
}

// The active vertex attributes: which, and where each comes from.
struct VertexLayout {
    uint32_t mask = 0;
    uint32_t count = 0;
    uint32_t index[kMaxAttributes];
    uint32_t format[kMaxAttributes];
    uint32_t stream[kMaxAttributes];
    uint32_t offset[kMaxAttributes];
};

bool current_layout(VertexLayout& layout) {
    static std::once_flag ffp_once, format_once;
    const ShaderState& shaders = shader_state();
    if (!shaders.programmable) {
        warn_once(ffp_once, "fixed-function vertex processing (FVF): not drawn yet");
        ++g_skipped_draws;
        return false;
    }
    for (uint32_t i = 0; i < kMaxAttributes; ++i) {
        const XbVertexShaderInput& in = shaders.attributes[i];
        if (in.format == kFormatNone) continue;
        if (!attribute_size(in.format)) {
            warn_once(format_once, "vertex attribute format not supported: attribute skipped");
            continue;
        }
        layout.mask |= 1u << i;
        layout.index[layout.count] = i;
        layout.format[layout.count] = in.format;
        layout.stream[layout.count] = in.stream;
        layout.offset[layout.count] = in.offset;
        ++layout.count;
    }
    return layout.count != 0;
}

thread_local std::vector<float> t_vertices;
thread_local std::vector<uint32_t> t_indices;

// Appends the primitive's indices as a back end primitive. `source` = null means
// vertices 0..count-1.
DrawInput::Primitive convert_primitive(uint32_t primitive, const uint32_t* source, uint32_t count,
                                       std::vector<uint32_t>& indices, bool& indexed) {
    auto at = [&](uint32_t i) { return source ? source[i] : i; };
    indexed = source != nullptr;
    switch (primitive) {
        case kPrimPoints: return DrawInput::Primitive::Points;
        case kPrimLines: return DrawInput::Primitive::Lines;
        case kPrimLineStrip: return DrawInput::Primitive::LineStrip;
        case kPrimTriangles: return DrawInput::Primitive::Triangles;
        case kPrimTriangleStrip:
        case kPrimQuadStrip:  // same vertex order as a triangle strip
            return DrawInput::Primitive::TriangleStrip;
        case kPrimTriangleFan:
        case kPrimPolygon: return DrawInput::Primitive::TriangleFan;
        case kPrimLineLoop:
            indexed = true;
            for (uint32_t i = 0; i < count; ++i) indices.push_back(at(i));
            if (count) indices.push_back(at(0));
            return DrawInput::Primitive::LineStrip;
        case kPrimQuads:
            indexed = true;
            for (uint32_t q = 0; q + 3 < count; q += 4)
                for (uint32_t i : {0u, 1u, 2u, 0u, 2u, 3u}) indices.push_back(at(q + i));
            return DrawInput::Primitive::Triangles;
        default: return DrawInput::Primitive::Invalid;
    }
}

// The deferred fog states -> NV2A fog registers, as D3D's lazy state update does
// (FUN_003f0e00 in CoD BRO). With a vertex program the fog distance is oFog.x.
void apply_fog_states() {
    const uint32_t* rs = render_state();
    if (!rs[kRsFogEnable]) {
        nv2a::write(kNvFogEnable, 0);
        return;
    }
    auto f = [rs](uint32_t index) {
        float value;
        std::memcpy(&value, &rs[index], 4);
        return value;
    };
    uint32_t gen_mode = rs[kRsRangeFogEnable] ? 1 : 2;  // radial or planar
    uint32_t mode;
    float bias, scale;
    switch (rs[kRsFogTableMode]) {
        case 0:  // none: the vertex fog value is the factor
            mode = kNvFogModeLinear, bias = 1.0f, scale = 1.0f, gen_mode = 0;
            break;
        case 3: {  // linear: (end - d) / (end - start)
            const float start = f(kRsFogStart), end = f(kRsFogEnd);
            const float inverse = end == start ? 8192.0f : 1.0f / (end - start);
            mode = kNvFogModeLinear, bias = end * inverse + 1.0f, scale = -inverse;
            break;
        }
        case 1:  // exp: e^(-density * d) = 2^(16 * scale * d)
            mode = kNvFogModeExp, bias = 1.5f, scale = f(kRsFogDensity) * -0.090168074f;
            break;
        default:  // exp2: e^(-(density * d)^2) = 2^(-32 * (scale * d)^2)
            mode = kNvFogModeExp2, bias = 1.5f, scale = f(kRsFogDensity) * -0.21233003f;
            break;
    }
    uint32_t params[3] = {0, 0, 0};
    std::memcpy(&params[0], &bias, 4);
    std::memcpy(&params[1], &scale, 4);
    nv2a::write(kNvFogGenMode, gen_mode);
    nv2a::write(kNvFogEnable, 1);
    nv2a::write(kNvFogMode, mode);
    nv2a::write(kNvFogParams, params, 3);
}

void submit(uint32_t primitive, const VertexLayout& layout, uint32_t vertex_count, const uint32_t* source_indices,
            uint32_t index_count) {
    static std::once_flag prim_once;
    if (!vertex_count || !begin_draw()) return;
    t_indices.clear();
    bool indexed = false;
    DrawInput input;
    input.primitive = convert_primitive(primitive, source_indices, source_indices ? index_count : vertex_count,
                                        t_indices, indexed);
    if (input.primitive == DrawInput::Primitive::Invalid) {
        warn_once(prim_once, "unknown primitive type: not drawn");
        ++g_skipped_draws;
        return;
    }
    if (indexed && !t_indices.empty()) {
        input.indices = t_indices.data();
        input.index_count = static_cast<uint32_t>(t_indices.size());
    } else if (source_indices) {
        input.indices = source_indices;
        input.index_count = index_count;
    }
    input.vertices = t_vertices.data();
    input.vertex_count = vertex_count;
    input.attribute_mask = layout.mask;
    input.depth_max = depth_buffer_max();
    apply_fog_states();
    platform::HostFloatEnv float_env;
    backend()->draw(input);
}

// SET_BEGIN_END ... INLINE_ARRAY ... END: vertices packed in attribute order.
void inline_draw(uint32_t primitive, const uint32_t* data, size_t dwords) {
    VertexLayout layout;
    if (!current_layout(layout)) return;
    uint32_t stride = 0;
    for (uint32_t a = 0; a < layout.count; ++a)
        stride = std::max(stride, layout.offset[a] + attribute_size(layout.format[a]));
    stride = (stride + 3) & ~3u;
    const uint32_t vertex_count = static_cast<uint32_t>(dwords * 4 / stride);
    const auto* bytes = reinterpret_cast<const uint8_t*>(data);
    t_vertices.resize(static_cast<size_t>(vertex_count) * layout.count * 4);
    float* out = t_vertices.data();
    for (uint32_t v = 0; v < vertex_count; ++v)
        for (uint32_t a = 0; a < layout.count; ++a, out += 4)
            fetch_attribute(layout.format[a], bytes + static_cast<size_t>(v) * stride + layout.offset[a], out);
    submit(primitive, layout, vertex_count, nullptr, 0);
}

}  // namespace

uint32_t skipped_draws() { return g_skipped_draws.load(std::memory_order_relaxed); }

void install_draw_handlers() { nv2a::set_inline_draw_handler(inline_draw); }

void draw_streams(uint32_t primitive, uint32_t start_vertex, uint32_t vertex_count, const uint16_t* indices) {
    VertexLayout layout;
    if (!current_layout(layout)) return;
    const ShaderState& shaders = shader_state();
    // Indexed: fetch the range the indices use, then rebase them.
    uint32_t first = start_vertex, last = start_vertex + vertex_count;  // [first, last)
    std::vector<uint32_t> rebased;
    if (indices) {
        if (!vertex_count) return;
        first = *std::min_element(indices, indices + vertex_count);
        last = *std::max_element(indices, indices + vertex_count) + 1u;
        rebased.resize(vertex_count);
        for (uint32_t i = 0; i < vertex_count; ++i) rebased[i] = indices[i] - first;
    }
    const uint32_t fetched = last - first;
    t_vertices.resize(static_cast<size_t>(fetched) * layout.count * 4);
    float* out = t_vertices.data();
    for (uint32_t v = 0; v < fetched; ++v) {
        for (uint32_t a = 0; a < layout.count; ++a, out += 4) {
            const XbStreamInput& stream = shaders.streams[layout.stream[a]];
            if (!stream.vertex_buffer) {  // no data: the defaults
                out[0] = out[1] = out[2] = 0.0f;
                out[3] = 1.0f;
                continue;
            }
            const auto* buffer = from_xbox_ptr<XbResource>(stream.vertex_buffer);
            const auto* base = from_xbox_ptr<uint8_t>(buffer->data | 0x80000000u);
            fetch_attribute(layout.format[a],
                            base + stream.offset + static_cast<size_t>(first + v) * stream.stride + layout.offset[a], out);
        }
    }
    submit(primitive, layout, fetched, indices ? rebased.data() : nullptr, vertex_count);
}

void draw_user_pointer(uint32_t primitive, uint32_t vertex_count, const void* data, uint32_t stride) {
    VertexLayout layout;
    if (!current_layout(layout)) return;
    const auto* bytes = static_cast<const uint8_t*>(data);
    t_vertices.resize(static_cast<size_t>(vertex_count) * layout.count * 4);
    float* out = t_vertices.data();
    for (uint32_t v = 0; v < vertex_count; ++v)
        for (uint32_t a = 0; a < layout.count; ++a, out += 4)
            fetch_attribute(layout.format[a], bytes + static_cast<size_t>(v) * stride + layout.offset[a], out);
    submit(primitive, layout, vertex_count, nullptr, 0);
}

namespace {

// --- The D3D draw functions -------------------------------------------------------------------

// Vertices start_vertex.. from the vertex streams.
void XB_STDCALL D3DDevice_DrawVertices(uint32_t primitive, uint32_t start_vertex, uint32_t vertex_count) {
    draw_streams(primitive, start_vertex, vertex_count, nullptr);
}

// 16-bit indices into the vertex streams.
void XB_STDCALL D3DDevice_DrawIndexedVertices(uint32_t primitive, uint32_t index_count, XbPtr indices) {
    draw_streams(primitive, 0, index_count, from_xbox_ptr<uint16_t>(indices));
}

// Stream 0 data from title memory.
void XB_STDCALL D3DDevice_DrawVerticesUP(uint32_t primitive, uint32_t vertex_count, XbPtr data, uint32_t stride) {
    draw_user_pointer(primitive, vertex_count, from_xbox_ptr<void>(data), stride);
}

}  // namespace

const xbox::KernelImpl kDrawFunctions[] = {
    D3D_IMPL(D3DDevice_DrawIndexedVertices),
    D3D_IMPL(D3DDevice_DrawVertices),
    D3D_IMPL(D3DDevice_DrawVerticesUP),
};
const size_t kDrawFunctionCount = std::size(kDrawFunctions);

void reset_draw_state(bool depth_buffer) {
    // NV2A values are OpenGL enums.
    constexpr uint32_t kGlLequal = 0x203, kGlAlways = 0x207, kGlKeep = 0x1E00, kGlFuncAdd = 0x8006, kGlSmooth = 0x1D01;
    nv2a::write(kNvColorMask, 0x01010101);
    nv2a::write(kNvDepthMask, 1);
    nv2a::write(kNvDepthFunc, kGlLequal);
    nv2a::write(kNvAlphaFunc, kGlAlways);
    nv2a::write(kNvBlendSrc, 1);  // GL_ONE
    nv2a::write(kNvBlendDst, 0);  // GL_ZERO
    nv2a::write(kNvBlendEquation, kGlFuncAdd);
    nv2a::write(kNvStencilFunc, kGlAlways);
    nv2a::write(kNvStencilFuncMask, 0xFF);
    nv2a::write(kNvStencilMask, 0xFF);
    nv2a::write(kNvStencilOpZFail, kGlKeep);
    nv2a::write(kNvStencilOpZPass, kGlKeep);
    nv2a::write(kNvShadeMode, kGlSmooth);
    uint32_t* rs = render_state();
    rs[kRsZEnable] = depth_buffer ? 1 : 0;
    rs[kRsCullMode] = kCullCcw;
    rs[kRsFrontFace] = kFrontFaceCw;
    rs[kRsFillMode] = kFillSolid;
    rs[kRsBackFillMode] = kFillSolid;
    rs[kRsStencilFail] = kGlKeep;
}

}  // namespace render
