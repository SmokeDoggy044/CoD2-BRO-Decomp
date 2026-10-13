// Title-specific patches: original functions that cannot run on a PC, replaced or
// turned into no-ops. Addresses are for CoD2: BRO default.xbe (XDK 5849).

#include "patches.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>
#include <vector>

#include "compiler.h"
#include "crash.h"
#include "d3d.h"
#include "dsound.h"
#include "filesystem.h"
#include "hooks.h"
#include "log.h"
#include "symbols.h"
#include "xbe.h"

namespace loader {

namespace {

// XAPI 001ede5f (unnamed in Ghidra), called during startup on retail (no debug
// monitor): finds the kernel's INIT section through its PE header at 0x80010000
// and rewrites GDT entry 1 (code selector 0x08) with SGDT + a direct GDT write, to
// limit code execution. Neither is possible or useful in user mode. With the saved
// descriptor left at zero, XAPI also skips putting it back (at 001ee829).
void XB_CDECL restrict_code_selector() {}

// XAPI XMountUtilityDrive (BOOL format_clean) -> BOOL: picks a cache partition through
// a table in raw disk sectors, formats it as FATX if needed and links Z: to it. The
// cache is a host folder here, so just link it (emptying it when asked to format).
uint32_t XB_STDCALL mount_utility_drive(uint32_t format_clean) {
    return xbox::mount_utility_drive(format_clean & 0xFF) == 0 ? 1 : 0;
}

struct Patch {
    uint32_t address;
    const void* replacement;
    const char* what;
};

const Patch kPatches[] = {
    {0x001ede5f, reinterpret_cast<const void*>(&restrict_code_selector), "XAPI code selector restriction"},
    {0x001e9b66, reinterpret_cast<const void*>(&mount_utility_drive), "XMountUtilityDrive: Z: = cache folder"},
};

}  // namespace

namespace {

struct SdkLibrary {
    const char* name;
    const char* section;  // XBE section holding the original library
    std::span<const xbox::KernelImpl> functions;
};

// SDK functions XbSymbolDatabase leaves unnamed in this title, by their XDK name.
struct SdkAlias {
    uint32_t address;
    const char* name;
};
const SdkAlias kSdkAliases[] = {
    {0x003e6210, "D3DDevice_BlockOnFence"},  // BlockOnTime(fence, 4)
    {0x003e6ea0, "D3DDevice_LoadVertexShaderProgram"},
    {0x003e5f20, "D3DDevice_GetTexture2"},
    {0x003e6200, "D3DDevice_BlockUntilIdle"},
    {0x003e6950, "D3DDevice_GetScreenSpaceOffset"},
};

// SDK functions that are safe to run as they are (no hardware access), left untrapped.
const uint32_t kSdkKeepOriginal[] = {
    0x003ebb20,  // D3D: returns 1 (called before D3D_SetPushBufferSize)
};

const SymbolTable* g_symbols = nullptr;

// Every function in a replaced library's section that we do not replace starts with
// "call sdk_trap", so reaching original SDK code (it would touch hardware) stops with
// its name. The return address is the trapped function's entry + 5; the caller's
// return address is the next stack slot.
[[noreturn]] PLATFORM_NOINLINE void XB_CDECL sdk_trap() {
    const auto* frame = static_cast<uint32_t*>(__builtin_frame_address(0));
    const uint32_t function = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(__builtin_return_address(0))) - 5;
    const uint32_t caller = frame[2];  // [ebp+8]: above our return address
    char message[200];
    std::snprintf(message, sizeof(message), "unimplemented SDK function %s",
                  g_symbols->describe(function).c_str());
    fatal_with_backtrace(message, caller, frame[0]);
}

void install_call(uint32_t address, const void* target) {
    auto* code = reinterpret_cast<uint8_t*>(uintptr_t{address});
    const uint32_t relative = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(target)) - (address + 5);
    code[0] = 0xE8;  // CALL rel32
    std::memcpy(code + 1, &relative, 4);
}

}  // namespace

bool install_sdk_replacements(const XbeImage& image, const SymbolTable& symbols) {
    g_symbols = &symbols;
    const SdkLibrary libraries[] = {
        {"DirectSound", "DSOUND", audio::dsound_functions()},
        {"Direct3D", "D3D", render::d3d_functions()},
    };
    bool ok = true;
    for (const SdkLibrary& library : libraries) {
        const XbeSectionHeader* section = nullptr;
        for (const XbeSectionHeader* s : image.sections())
            if (image.section_name(*s) == library.section) section = s;
        if (!section) continue;  // the title does not use this library

        // Trap first; replacements then overwrite the entries they own. Functions
        // shorter than a CALL are left alone (they would clobber their neighbour).
        const std::vector<uint32_t> entries =
            symbols.in_range(section->virtual_addr, section->virtual_addr + section->virtual_size);
        for (size_t i = 0; i < entries.size(); ++i) {
            if (std::find(std::begin(kSdkKeepOriginal), std::end(kSdkKeepOriginal), entries[i]) !=
                std::end(kSdkKeepOriginal))
                continue;
            const uint32_t next = i + 1 < entries.size() ? entries[i + 1] : section->virtual_addr + section->virtual_size;
            if (next - entries[i] >= 5) install_call(entries[i], reinterpret_cast<const void*>(&sdk_trap));
        }

        int installed = 0, missing = 0;
        for (const xbox::KernelImpl& function : library.functions) {
            std::vector<uint32_t> addresses = symbols.find(function.name);
            for (const SdkAlias& alias : kSdkAliases)
                if (std::strcmp(alias.name, function.name) == 0) addresses.push_back(alias.address);
            if (addresses.empty()) {
                LOG_WARN("patches", "%s: %s not found in the symbols; not replaced", library.name, function.name);
                ++missing;
                continue;
            }
            for (uint32_t address : addresses) install_jump(address, function.address);
            ++installed;
        }
        LOG_INFO("patches", "%s: %d functions replaced, %d not found, %zu others trapped", library.name, installed,
                 missing, entries.size() - installed);
        if (installed == 0) ok = false;
    }
    return ok;
}

void install_title_patches() {
    for (const Patch& patch : kPatches) {
        install_jump(patch.address, patch.replacement);
        LOG_DEBUG("patches", "%08x: %s", patch.address, patch.what);
    }
}

}  // namespace loader
