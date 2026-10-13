#pragma once

#include <cstdint>

namespace loader {

class SymbolTable;
class XbeImage;

// Reports faults (SIGSEGV, SIGILL, ...) with registers and an Xbox-side backtrace,
// then exits.
void install_crash_handler(const XbeImage& image, const SymbolTable& symbols);

// Logs a backtrace by following the EBP chain, starting at `eip`.
void log_backtrace(uint32_t eip, uint32_t ebp);

// Logs `message` plus a backtrace and exits. For fatal conditions found in loader
// code, e.g. a call to an unimplemented kernel export.
[[noreturn]] void fatal_with_backtrace(const char* message, uint32_t eip, uint32_t ebp);

}  // namespace loader
