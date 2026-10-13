#pragma once

// Xbox Direct3D 8 structures and constants as they appear in title memory (XDK 5849).
// Reference: third_party/xbox-includes/include/d3d8types.h.

#include <cstdint>

#include "types.h"

namespace render {

using xbox::XbPtr;

constexpr uint32_t kD3DOk = 0;
constexpr uint32_t kD3DErrInvalidCall = 0x8876086C;

// D3DResource: every resource starts with these three fields. Common holds the
// reference count and type; Data is the resource's memory as a physical address
// (CPU address = contiguous base + Data) once registered.
struct XbResource {
    uint32_t common;
    uint32_t data;
    uint32_t lock;
};
static_assert(sizeof(XbResource) == 12);

constexpr uint32_t kCommonRefcountMask = 0x0000FFFF;
constexpr uint32_t kCommonTypeMask = 0x00070000;
constexpr uint32_t kCommonTypeVertexBuffer = 0x00000000;
constexpr uint32_t kCommonTypeIndexBuffer = 0x00010000;
constexpr uint32_t kCommonTypePushBuffer = 0x00020000;
constexpr uint32_t kCommonTypePalette = 0x00030000;
constexpr uint32_t kCommonTypeTexture = 0x00040000;
constexpr uint32_t kCommonTypeSurface = 0x00050000;
constexpr uint32_t kCommonTypeFixup = 0x00060000;
constexpr uint32_t kCommonD3DCreated = 0x01000000;  // D3D owns the memory

// D3DPixelContainer (textures, surfaces, volumes).
struct XbPixelContainer {
    XbResource resource;
    uint32_t format;  // kFormat* bitfields
    uint32_t size;    // linear (non power of two) sizes only, else 0
};
static_assert(sizeof(XbPixelContainer) == 20);

// D3DSurface: a pixel container plus the texture it belongs to (or null).
struct XbSurface {
    XbPixelContainer container;
    XbPtr parent;
};
static_assert(sizeof(XbSurface) == 24);

// XbPixelContainer::format
constexpr uint32_t kFormatDmaChannelA = 0x00000001;
constexpr uint32_t kFormatCubemap = 0x00000004;
constexpr uint32_t kFormatDimensionShift = 4;
constexpr uint32_t kFormatFormatShift = 8;
constexpr uint32_t kFormatFormatMask = 0x0000FF00;
constexpr uint32_t kFormatMipmapShift = 16;
constexpr uint32_t kFormatUSizeShift = 20;
constexpr uint32_t kFormatVSizeShift = 24;
constexpr uint32_t kFormatPSizeShift = 28;

// XbPixelContainer::size
constexpr uint32_t kSizeWidthMask = 0x00000FFF;  // width - 1
constexpr uint32_t kSizeHeightMask = 0x00FFF000;  // height - 1
constexpr uint32_t kSizeHeightShift = 12;
constexpr uint32_t kSizePitchShift = 24;  // pitch / 64 - 1

// D3DFORMAT values used for render targets and depth buffers.
constexpr uint32_t kFmtA8R8G8B8 = 0x06;
constexpr uint32_t kFmtX8R8G8B8 = 0x07;
constexpr uint32_t kFmtLinA8R8G8B8 = 0x12;
constexpr uint32_t kFmtLinX8R8G8B8 = 0x1E;
constexpr uint32_t kFmtD24S8 = 0x2A;
constexpr uint32_t kFmtF24S8 = 0x2B;
constexpr uint32_t kFmtD16 = 0x2C;
constexpr uint32_t kFmtLinD24S8 = 0x2E;
constexpr uint32_t kFmtLinD16 = 0x30;

// D3DPRESENT_PARAMETERS (the PC D3D8 layout plus four Xbox fields).
struct XbPresentParameters {
    uint32_t back_buffer_width;
    uint32_t back_buffer_height;
    uint32_t back_buffer_format;
    uint32_t back_buffer_count;
    uint32_t multisample_type;
    uint32_t swap_effect;
    XbPtr device_window;
    uint32_t windowed;
    uint32_t enable_auto_depth_stencil;
    uint32_t auto_depth_stencil_format;
    uint32_t flags;
    uint32_t refresh_rate;
    uint32_t presentation_interval;
    XbPtr buffer_surfaces[3];
    XbPtr depth_stencil_surface;
};
static_assert(sizeof(XbPresentParameters) == 0x44);

// D3DClear flags.
constexpr uint32_t kClearTargetMask = 0xF0;  // R, G, B, A
constexpr uint32_t kClearZBuffer = 0x01;
constexpr uint32_t kClearStencil = 0x02;

// D3DVBLANKDATA, passed to the vertical blank callback.
struct XbVBlankData {
    uint32_t vblank;
    uint32_t swap;
    uint32_t flags;
};
static_assert(sizeof(XbVBlankData) == 12);

// D3DFIELD_STATUS
struct XbFieldStatus {
    uint32_t field;  // 1 odd, 2 even, 3 progressive
    uint32_t vblank_count;
};
static_assert(sizeof(XbFieldStatus) == 8);

// The device object. Titles hold a pointer to it (D3D__pDevice) and inlined header
// code reads a few fields directly; ours is a zeroed block that keeps those current.
// Offsets are for XDK 5849 (CDevice).
constexpr uint32_t kDeviceSize = 0x2500;
constexpr uint32_t kDeviceRenderTarget = 0x1A04;  // D3DSurface*
constexpr uint32_t kDeviceDepthStencil = 0x1A08;  // D3DSurface* or null
constexpr uint32_t kDeviceBackBuffer = 0x1A14;    // D3DSurface*
constexpr uint32_t kDeviceSwapCount = 0x2478;

}  // namespace render
