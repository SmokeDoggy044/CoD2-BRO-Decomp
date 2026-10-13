// Kernel exports that need nothing from the loader: time, strings, debug output and
// simple variables. Signatures follow third_party/xbox-includes/include/xboxkrnl.h.
//
// 64-bit values are returned as int64_t rather than LARGE_INTEGER: GCC returns
// structs through a hidden pointer on i386, MSVC (and the Xbox) in EDX:EAX.

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

#include "compiler.h"
#include "kernel_internal.h"
#include "log.h"
#include "filesystem.h"
#include "memory.h"
#include "types.h"

namespace xbox {

namespace {

std::atomic<bool> g_trace{false};
KernelConfig g_config{};

const auto g_start = std::chrono::steady_clock::now();

// Rate of the Xbox ACPI timer, as used by Cxbx-Reloaded. Any value works as long as
// counter and frequency agree.
constexpr int64_t kPerformanceFrequency = 3375000;

// 100ns intervals between 1601-01-01 and 1970-01-01.
constexpr int64_t kUnixEpochAsFileTime = 116444736000000000LL;

// --- Variables -------------------------------------------------------------------

volatile uint32_t KeTickCount = 0;
XbKrnlVersion XboxKrnlVersion = {1, 0, 5838, 1};
XbAnsiString XeImageFileName = {};
char g_image_file_name[128] = {};
uint32_t LaunchDataPage = 0;  // PLAUNCH_DATA_PAGE; null means "launched from the dashboard"

void tick_thread() {
    for (;;) {
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - g_start);
        KeTickCount = static_cast<uint32_t>(ms.count());
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

std::mutex g_shutdown_mutex;
std::vector<XbShutdownRegistration*> g_shutdown_registrations;

// --- Functions -------------------------------------------------------------------

uint32_t XB_CDECL DbgPrint(const char* format, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, format);
    std::vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    size_t length = std::strlen(buffer);
    while (length > 0 && (buffer[length - 1] == '\n' || buffer[length - 1] == '\r'))
        buffer[--length] = '\0';
    LOG_INFO("dbgprint", "%s", buffer);
    return 0;  // STATUS_SUCCESS
}

// Only recorded; the list links inside the struct are left alone.
void XB_STDCALL HalRegisterShutdownNotification(XbShutdownRegistration* registration, uint32_t register_it) {
    std::lock_guard lock(g_shutdown_mutex);
    auto& list = g_shutdown_registrations;
    list.erase(std::remove(list.begin(), list.end(), registration), list.end());
    if (register_it & 0xFF) list.push_back(registration);
    KTRACE("HalRegisterShutdownNotification(%08x routine=%08x priority=%d, %s)", to_xbox_ptr(registration),
           registration->notification_routine, registration->priority, (register_it & 0xFF) ? "register" : "unregister");
}

int64_t XB_STDCALL KeQueryPerformanceCounter() {
    const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::steady_clock::now() - g_start)
                        .count();
    return static_cast<int64_t>(static_cast<long double>(ns) * kPerformanceFrequency / 1e9L);
}

int64_t XB_STDCALL KeQueryPerformanceFrequency() { return kPerformanceFrequency; }

void XB_STDCALL KeQuerySystemTime(int64_t* current_time) {
    *current_time = current_system_time();
    KTRACE("KeQuerySystemTime() -> %lld", static_cast<long long>(*current_time));
}

uint32_t XB_STDCALL RtlEqualString(const XbAnsiString* string1, const XbAnsiString* string2,
                                   uint32_t case_insensitive) {
    if (string1->length != string2->length) return 0;
    const char* a = from_xbox_ptr<const char>(string1->buffer);
    const char* b = from_xbox_ptr<const char>(string2->buffer);
    auto upper = [](char c) { return (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c; };
    for (uint16_t i = 0; i < string1->length; ++i)
        if (case_insensitive & 0xFF ? upper(a[i]) != upper(b[i]) : a[i] != b[i]) return 0;
    return 1;
}

// NTSTATUS -> Win32 error, as NT maps them. Only statuses this kernel returns or
// file/IO code is likely to see; extend as new ones show up (they are logged).
uint32_t XB_STDCALL RtlNtStatusToDosError(uint32_t status) {
    struct Mapping {
        uint32_t status;
        uint32_t error;
    };
    static constexpr Mapping kMappings[] = {
        {0x00000000, 0},    // STATUS_SUCCESS
        {0x00000102, 258},  // STATUS_TIMEOUT -> WAIT_TIMEOUT
        {0x00000103, 997},  // STATUS_PENDING -> ERROR_IO_PENDING
        {0x80000005, 234},  // STATUS_BUFFER_OVERFLOW -> ERROR_MORE_DATA
        {0x80000006, 18},   // STATUS_NO_MORE_FILES
        {0xC0000002, 1},    // STATUS_NOT_IMPLEMENTED -> ERROR_INVALID_FUNCTION
        {0xC0000008, 6},    // STATUS_INVALID_HANDLE
        {0xC000000D, 87},   // STATUS_INVALID_PARAMETER
        {0xC000000F, 2},    // STATUS_NO_SUCH_FILE -> ERROR_FILE_NOT_FOUND
        {0xC0000010, 1},    // STATUS_INVALID_DEVICE_REQUEST -> ERROR_INVALID_FUNCTION
        {0xC0000011, 38},   // STATUS_END_OF_FILE -> ERROR_HANDLE_EOF
        {0xC0000017, 8},    // STATUS_NO_MEMORY -> ERROR_NOT_ENOUGH_MEMORY
        {0xC0000022, 5},    // STATUS_ACCESS_DENIED
        {0xC0000023, 122},  // STATUS_BUFFER_TOO_SMALL -> ERROR_INSUFFICIENT_BUFFER
        {0xC0000024, 6},    // STATUS_OBJECT_TYPE_MISMATCH -> ERROR_INVALID_HANDLE
        {0xC0000033, 123},  // STATUS_OBJECT_NAME_INVALID -> ERROR_INVALID_NAME
        {0xC0000034, 2},    // STATUS_OBJECT_NAME_NOT_FOUND -> ERROR_FILE_NOT_FOUND
        {0xC0000035, 183},  // STATUS_OBJECT_NAME_COLLISION -> ERROR_ALREADY_EXISTS
        {0xC0000039, 161},  // STATUS_OBJECT_PATH_INVALID -> ERROR_BAD_PATHNAME
        {0xC000003A, 3},    // STATUS_OBJECT_PATH_NOT_FOUND -> ERROR_PATH_NOT_FOUND
        {0xC0000043, 32},   // STATUS_SHARING_VIOLATION
        {0xC0000046, 288},  // STATUS_MUTANT_NOT_OWNED -> ERROR_NOT_OWNER
        {0xC000007F, 112},  // STATUS_DISK_FULL
        {0xC00000BA, 5},    // STATUS_FILE_IS_A_DIRECTORY -> ERROR_ACCESS_DENIED
        {0xC0000101, 145},  // STATUS_DIRECTORY_NOT_EMPTY -> ERROR_DIR_NOT_EMPTY
        {0xC0000103, 267},  // STATUS_NOT_A_DIRECTORY -> ERROR_DIRECTORY
    };
    for (const Mapping& m : kMappings)
        if (m.status == status) return m.error;
    if ((status & 0x3FFF0000) == 0x00070000) return status & 0xFFFF;  // FACILITY_WIN32
    LOG_DEBUG("kernel", "RtlNtStatusToDosError: no mapping for %08x", status);
    return 317;  // ERROR_MR_MID_NOT_FOUND, NT's answer for unknown statuses
}

void XB_STDCALL RtlInitAnsiString(XbAnsiString* destination, const char* source) {
    const size_t length = source ? std::strlen(source) : 0;
    destination->length = static_cast<uint16_t>(length);
    destination->maximum_length = static_cast<uint16_t>(source ? length + 1 : 0);
    destination->buffer = to_xbox_ptr(source);
    KTRACE("RtlInitAnsiString(\"%s\")", source ? source : "(null)");
}

const KernelImpl kImpls[] = {
    KIMPL(DbgPrint),
    KIMPL(HalRegisterShutdownNotification),
    KIMPL(KeQueryPerformanceCounter),
    KIMPL(KeQueryPerformanceFrequency),
    KIMPL(KeQuerySystemTime),
    KIMPL(RtlEqualString),
    KIMPL(RtlInitAnsiString),
    KIMPL(RtlNtStatusToDosError),
    KIMPL_DATA(KeTickCount),
    KIMPL_DATA(LaunchDataPage),
    KIMPL_DATA(XboxKrnlVersion),
    KIMPL_DATA(XeImageFileName),
};

}  // namespace

std::span<const KernelImpl> basic_kernel_impls() { return kImpls; }

std::span<const KernelImpl> kernel_impls() {
    static const std::vector<KernelImpl> all = [] {
        std::vector<KernelImpl> impls;
        for (std::span<const KernelImpl> part :
             {basic_kernel_impls(), object_kernel_impls(), sync_kernel_impls(), dispatcher_kernel_impls(),
              settings_kernel_impls(), memory_kernel_impls(), filesystem_kernel_impls()})
            impls.insert(impls.end(), part.begin(), part.end());
        return impls;
    }();
    return all;
}

bool kernel_trace_enabled() { return g_trace.load(std::memory_order_relaxed); }

std::vector<XbShutdownRegistration*> shutdown_notifications() {
    std::lock_guard lock(g_shutdown_mutex);
    auto list = g_shutdown_registrations;
    std::stable_sort(list.begin(), list.end(), [](auto* a, auto* b) { return a->priority > b->priority; });
    return list;
}

uint32_t nt_status_to_dos_error(uint32_t status) { return RtlNtStatusToDosError(status); }

uint32_t current_kthread() { return g_config.current_thread ? g_config.current_thread() : 0; }

const KernelConfig& kernel_config() { return g_config; }

int64_t to_system_time(std::chrono::system_clock::time_point time) {
    const auto since_unix = std::chrono::duration_cast<std::chrono::nanoseconds>(time.time_since_epoch()).count();
    return kUnixEpochAsFileTime + since_unix / 100;
}

int64_t current_system_time() { return to_system_time(std::chrono::system_clock::now()); }

bool kernel_init(const KernelConfig& config) {
    g_config = config;
    g_trace = config.trace_calls;
    std::snprintf(g_image_file_name, sizeof(g_image_file_name), "%s", config.image_file_name);
    RtlInitAnsiString(&XeImageFileName, g_image_file_name);
    std::thread(tick_thread).detach();
    return memory_init(config.memory) && filesystem_init(config);
}

}  // namespace xbox
