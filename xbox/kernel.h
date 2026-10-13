#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

namespace xbox {

// --- Export table (generated from tools/data + xboxkrnl.h) ---------------------

enum class ExportKind : uint8_t { Stdcall, Fastcall, Cdecl, Data, Unknown };

struct KernelExport {
    uint16_t ordinal;
    const char* name;
    ExportKind kind;
    uint16_t stack_bytes;  // argument bytes the callee pops
};

constexpr uint32_t kMaxKernelOrdinal = 378;

const KernelExport* find_kernel_export(uint32_t ordinal);
const KernelExport* find_kernel_export(const char* name);
const char* kernel_export_name(uint32_t ordinal);  // "?" if unknown

// --- Implementations -----------------------------------------------------------

// A kernel function or variable we provide, matched to the import table by name.
struct KernelImpl {
    const char* name;
    void* address;
};

// The portable implementations in this library.
std::span<const KernelImpl> kernel_impls();

// Memory layout and budget. Physical memory is one page budget shared by the image,
// contiguous allocations and committed virtual memory, as on the Xbox.
struct MemoryConfig {
    uint32_t physical_bytes = 64 * 1024 * 1024;  // retail Xbox: 64 MB (devkit: 128 MB)
    uint32_t contiguous_base = 0x80000000;       // virtual address of physical address 0
    uint32_t virtual_base = 0x10000000;          // range for NtAllocateVirtualMemory
    uint32_t virtual_end = 0x50000000;
};

struct KernelConfig {
    const char* image_file_name;  // XeImageFileName, e.g. "\\Device\\CdRom0\\default.xbe"
    bool trace_calls;             // log every kernel call at trace level
    // What the kernel needs from whoever runs the title's code (the loader):
    // Xbox address of the calling thread's KTHREAD (what KeGetCurrentThread returns).
    uint32_t (*current_thread)();
    // The calling thread's IRQL byte (KPCR.Irql, which XAPI reads through FS), or null.
    uint8_t* (*current_irql)();
    // Prepares a thread the kernel started itself (timers/DPCs) to run title code.
    void (*attach_kernel_thread)();
    // Calls a title function (stdcall) with `count` 32-bit arguments. Works from
    // kernel threads and from inside a kernel call made by the title (nested).
    void (*call_guest)(uint32_t function, const uint32_t* args, uint32_t count);
    MemoryConfig memory;
    // Host folders behind the Xbox devices. Empty = device not available.
    std::filesystem::path cdrom_root;  // \Device\CdRom0 (D:), read-only: the game's files
    std::filesystem::path data_root;   // writable: <data>/E = partition 1 (E:, T:, U:), <data>/Z = cache (Z:)
};

// Returns false if the memory layout cannot be set up.
bool kernel_init(const KernelConfig& config);
bool kernel_trace_enabled();
uint32_t current_kthread();
const KernelConfig& kernel_config();

struct XbShutdownRegistration;
// Routines registered with HalRegisterShutdownNotification, highest priority first.
// Whoever implements shutdown (HalReturnToFirmware) calls them.
std::vector<XbShutdownRegistration*> shutdown_notifications();

}  // namespace xbox
