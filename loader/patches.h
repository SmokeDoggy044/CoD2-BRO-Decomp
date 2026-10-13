#pragma once

#include <cstdint>

namespace loader {

// The title's D3D__pDevice global (CoD2: BRO, XDK 5849 LTCG).
constexpr uint32_t kTitleD3DDevicePointer = 0x003f8318;
// Its D3D__RenderState array.
constexpr uint32_t kTitleD3DRenderState = 0x003f8328;
// Its D3D_g_DeferredTextureState array.
constexpr uint32_t kTitleD3DTextureState = 0x003f5c68;
// XPP's g_DeviceType_Gamepad (XDEVICE_TYPE_GAMEPAD).
constexpr uint32_t kTitleGamepadType = 0x007a9f78;

// Redirects the title's functions that cannot run on a PC (privileged instructions,
// direct kernel/hardware access) to replacements. Call after the image is mapped.
void install_title_patches();

class SymbolTable;
class XbeImage;

// Redirects the SDK libraries the port replaces (DirectSound, Direct3D), found by name
// in the Ghidra symbols. Every other function in those libraries' sections traps with
// its name when called. Returns false if a library is present but nothing was found.
bool install_sdk_replacements(const XbeImage& image, const SymbolTable& symbols);

}  // namespace loader
