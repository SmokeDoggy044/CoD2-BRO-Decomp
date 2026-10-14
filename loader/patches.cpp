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
#include "xinput.h"
#include "xonline.h"
#include "devices.h"
#include "dsound.h"
#include "filesystem.h"
#include "float_env.h"
#include "hooks.h"
#include "kernel.h"
#include "log.h"
#include "symbols.h"
#include "xbe.h"

#include <sys/mman.h>

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

// RDTSC sites in title code (Ghidra: every RDTSC outside the SDK sections) and how
// many bytes from the RDTSC on are moved to the trampoline: RDTSC plus the stores
// of its result after it, which use no relative addressing and are no branch target.
// The title assumes the Xbox CPU's 733 MHz (FUN_002960a0 divides by 733333.3; XAPI
// QueryPerformanceCounter is RDTSC with frequency 733333333); the host's counter
// runs several times faster, and so would the game.
struct RdtscSite {
    uint32_t address;
    uint8_t length;
};
const RdtscSite kRdtscSites[] = {
    {0x000643ad, 6}, {0x0006441a, 6}, {0x00064584, 6}, {0x0006469e, 6}, {0x00064f2e, 6}, {0x00064f98, 6},
    {0x00064fe8, 6}, {0x00065139, 6}, {0x000651ea, 6}, {0x00065329, 6},
    {0x001ed6ce, 7},  // XAPI QueryPerformanceCounter: mov [ecx],eax; mov [ecx+4],edx
    {0x001fda73, 6}, {0x0020d34a, 6}, {0x0020d37e, 6}, {0x002960ba, 6}, {0x002960e3, 6}, {0x00298185, 6},
    {0x002981ae, 6}, {0x002b1caf, 6}, {0x002b1ce9, 6}, {0x0030ba24, 6}, {0x0030bc51, 6},
    {0x0032d893, 5},  // mov [esp],eax
    {0x0032ebca, 6}, {0x0032ec52, 6}, {0x00331fc5, 6}, {0x0033228a, 6},
};

int64_t XB_CDECL title_rdtsc() { return xbox::cpu_time_stamp_counter(); }

void put_rel32(uint8_t* at, uint8_t opcode, const void* target) {
    const uint32_t from = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(at)) + 5;
    const uint32_t relative = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(target)) - from;
    at[0] = opcode;
    std::memcpy(at + 1, &relative, 4);
}

// Each site: JMP to a trampoline that does pushfd; push ecx; call title_rdtsc;
// pop ecx; popfd; <the moved instructions>; jmp back.
void install_rdtsc_patches() {
    constexpr size_t kTrampolineSize = 32;
    auto* pool = static_cast<uint8_t*>(mmap(nullptr, std::size(kRdtscSites) * kTrampolineSize,
                                            PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    if (pool == MAP_FAILED) {
        LOG_ERROR("patches", "no memory for the RDTSC trampolines; the title runs at the host's TSC rate");
        return;
    }
    int installed = 0;
    for (size_t i = 0; i < std::size(kRdtscSites); ++i) {
        const RdtscSite& site = kRdtscSites[i];
        auto* code = reinterpret_cast<uint8_t*>(uintptr_t{site.address});
        if (code[0] != 0x0F || code[1] != 0x31) {
            LOG_WARN("patches", "%08x: not RDTSC; left alone", site.address);
            continue;
        }
        uint8_t* t = pool + i * kTrampolineSize;
        *t++ = 0x9C;  // pushfd
        *t++ = 0x51;  // push ecx
        put_rel32(t, 0xE8, reinterpret_cast<const void*>(&title_rdtsc));
        t += 5;
        *t++ = 0x59;  // pop ecx
        *t++ = 0x9D;  // popfd
        std::memcpy(t, code + 2, site.length - 2u);
        t += site.length - 2;
        put_rel32(t, 0xE9, code + site.length);
        put_rel32(code, 0xE9, pool + i * kTrampolineSize);
        std::memset(code + 5, 0x90, site.length - 5u);  // NOP
        ++installed;
    }
    LOG_INFO("patches", "RDTSC: %d sites run at the Xbox's 733 MHz", installed);
}

// --- Mouse look ---------------------------------------------------------------------
//
// The Xbox build feeds its right stick through Quake 3's mouse path (in_joystick:
// IN_JoyMove posts SE_MOUSE events, CL_MouseMove FUN_002b39e0 applies stick
// acceleration and a 370 deg/s turn limit), so a real mouse cannot go through there.
// Instead CL_CreateCmd's call to CL_MouseMove goes through a thunk that first adds
// the mouse motion to cl.viewangles the way PC CoD does, under the same conditions
// CL_MouseMove checks.

constexpr uint32_t kCallMouseMove = 0x002c7ad7;  // in CL_CreateCmd FUN_002c7a90
constexpr uint32_t kMouseMove = 0x002b39e0;

template <typename T>
T& title(uint32_t address) {
    return *reinterpret_cast<T*>(uintptr_t{address});
}
// A cvar's float / integer value (cvar_t: value at +0x1C, integer at +0x20).
float cvar_value(uint32_t cvar_pointer) { return title<float>(title<uint32_t>(cvar_pointer) + 0x1C); }
int32_t cvar_integer(uint32_t cvar_pointer) { return title<int32_t>(title<uint32_t>(cvar_pointer) + 0x20); }

float g_mouse_degrees_per_count = 0.11f;

void XB_CDECL mouse_look() {
    float dx, dy;
    input::take_mouse_motion(dx, dy);
    if (dx == 0.0f && dy == 0.0f) return;
    // Player state: view locked (0x4000) unless one of 0x106000 allows looking.
    if ((title<uint32_t>(0x009601ac) & 0x4000) && !(title<uint32_t>(0x00960240) & 0x106000)) return;
    if (title<int32_t>(0x00933fb8) != 0) return;  // in a vehicle: its own controls
    platform::HostFloatEnv float_env;
    // cl.cgameSensitivity: cg.zoomSensitivity, set each frame by CG_DrawActiveFrame
    // (FUN_0028b340); lower while aiming down sights, 0 before the first frame.
    const float scale = g_mouse_degrees_per_count * title<float>(0x00961774);
    title<float>(0x0096179c) -= dx * scale;  // cl.viewangles[YAW]
    if (title<int32_t>(0x009341b0) != 0 || cvar_integer(0x0093f808) != 0) {  // cl_freelook
        const float invert = cvar_value(0x00929c0c) < 0.0f ? -1.0f : 1.0f;  // m_pitch sign = invert Y
        title<float>(0x00961798) += dy * scale * invert;  // cl.viewangles[PITCH]
    }
}

// pushad; pushfd; call mouse_look; popfd; popad; jmp CL_MouseMove
void install_mouse_look() {
    auto* site = reinterpret_cast<uint8_t*>(uintptr_t{kCallMouseMove});
    uint32_t relative;
    std::memcpy(&relative, site + 1, 4);
    if (site[0] != 0xE8 || kCallMouseMove + 5 + relative != kMouseMove) {
        LOG_WARN("patches", "%08x: not the call to CL_MouseMove; no mouse look", kCallMouseMove);
        return;
    }
    auto* thunk = static_cast<uint8_t*>(
        mmap(nullptr, 64, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    if (thunk == MAP_FAILED) return;
    uint8_t* t = thunk;
    *t++ = 0x60;  // pushad
    *t++ = 0x9C;  // pushfd
    put_rel32(t, 0xE8, reinterpret_cast<const void*>(&mouse_look));
    t += 5;
    *t++ = 0x9D;  // popfd
    *t++ = 0x61;  // popad
    put_rel32(t, 0xE9, reinterpret_cast<const void*>(uintptr_t{kMouseMove}));
    put_rel32(site, 0xE8, thunk);
    LOG_INFO("patches", "mouse look: %.3f degrees per count", g_mouse_degrees_per_count);
}

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
    {0x00379ebb, "XOnlineSilentLogon"},  // GetUsers, first account, Logon; 0x8015108x errors
};

// SDK functions that are safe to run as they are (no hardware access), left untrapped.
const uint32_t kSdkKeepOriginal[] = {
    0x003ebb20,  // D3D: returns 1 (called before D3D_SetPushBufferSize)
    0x00396f5f,  // XONLINE: a static initializer (_cinit)
};

// The same, by SDK name (any title): thin wrappers around XAPI.
const char* const kSdkKeepOriginalNames[] = {
    "D3D_AllocContiguousMemory",  // XMemAlloc with D3D's attributes
    "D3D_FreeContiguousMemory",   // XMemFree
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
        {"Input", "XPP", input::xpp_functions()},
        {"Xbox Live", "XONLINE", online::xonline_functions()},
        {"Network", "XNET", {}},
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
        std::vector<uint32_t> keep(std::begin(kSdkKeepOriginal), std::end(kSdkKeepOriginal));
        for (const char* name : kSdkKeepOriginalNames)
            for (uint32_t address : symbols.find(name)) keep.push_back(address);
        for (size_t i = 0; i < entries.size(); ++i) {
            if (std::find(keep.begin(), keep.end(), entries[i]) != keep.end()) continue;
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
        if (installed == 0 && !library.functions.empty()) ok = false;
    }
    return ok;
}

// XAPI XLaunchNewImageA calls the first non-null entry of a launch hook table
// (0x00897908-0x00897910, static data) instead of its own path. XONLINE puts
// FUN_0037da38 there: the same launch plus Xbox Live dashboard handling and cleanup.
// Without it XAPI writes the LaunchDataPage and calls HalReturnToFirmware itself.
constexpr uint32_t kLaunchHookTable = 0x00897908;
constexpr uint32_t kLaunchHookTableEnd = 0x00897910;

void install_title_patches(const TitlePatchConfig& config) {
    g_mouse_degrees_per_count = config.mouse_degrees_per_count;
    for (const Patch& patch : kPatches) {
        install_jump(patch.address, patch.replacement);
        LOG_DEBUG("patches", "%08x: %s", patch.address, patch.what);
    }
    std::memset(reinterpret_cast<void*>(uintptr_t{kLaunchHookTable}), 0, kLaunchHookTableEnd - kLaunchHookTable);
    install_rdtsc_patches();
    install_mouse_look();
}

}  // namespace loader
