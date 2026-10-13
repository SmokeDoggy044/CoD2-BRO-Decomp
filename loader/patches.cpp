// Title-specific patches: original functions that cannot run on a PC, replaced or
// turned into no-ops. Addresses are for CoD2: BRO default.xbe (XDK 5849).

#include "patches.h"

#include <cstdint>

#include "compiler.h"
#include "filesystem.h"
#include "hooks.h"
#include "log.h"

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

void install_title_patches() {
    for (const Patch& patch : kPatches) {
        install_jump(patch.address, patch.replacement);
        LOG_DEBUG("patches", "%08x: %s", patch.address, patch.what);
    }
}

}  // namespace loader
