#include "crash.h"

#include <signal.h>
#include <sys/ucontext.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "log.h"
#include "symbols.h"
#include "thread_env.h"
#include "xbe.h"

namespace loader {

namespace {

const XbeImage* g_image = nullptr;
const SymbolTable* g_symbols = nullptr;

std::string where(uint32_t address) {
    if (!g_image || !g_image->contains(address)) {
        // The Ghidra symbols only cover the XBE; don't attach them to our own code.
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "%08x [host]", address);
        return buffer;
    }
    std::string text = g_symbols->describe(address);
    if (const XbeSectionHeader* section = g_image->find_section(address))
        text += " [" + g_image->section_name(*section) + "]";
    return text;
}

bool on_xbox_stack(uint32_t address) {
    const ThreadEnv* env = current_thread_env();
    return env && address >= env->stack_limit && address + 8 <= env->stack_base;
}

void handle_signal(int signal, siginfo_t* info, void* context) {
    const auto& gregs = static_cast<ucontext_t*>(context)->uc_mcontext.gregs;
    const auto reg = [&](int index) { return static_cast<uint32_t>(gregs[index]); };
    LOG_ERROR("crash", "%s at %s, fault address %08x", strsignal(signal), where(reg(REG_EIP)).c_str(),
              static_cast<uint32_t>(reinterpret_cast<uintptr_t>(info->si_addr)));
    LOG_ERROR("crash", "eax=%08x ebx=%08x ecx=%08x edx=%08x esi=%08x edi=%08x", reg(REG_EAX), reg(REG_EBX),
              reg(REG_ECX), reg(REG_EDX), reg(REG_ESI), reg(REG_EDI));
    LOG_ERROR("crash", "esp=%08x ebp=%08x eflags=%08x", reg(REG_ESP), reg(REG_EBP), reg(REG_EFL));
    log_backtrace(reg(REG_EIP), reg(REG_EBP));
    _exit(128 + signal);
}

}  // namespace

void install_crash_handler(const XbeImage& image, const SymbolTable& symbols) {
    g_image = &image;
    g_symbols = &symbols;

    // Run the handler on its own stack in case the Xbox stack is what broke.
    static char alt_stack[64 * 1024];
    stack_t ss{};
    ss.ss_sp = alt_stack;
    ss.ss_size = sizeof(alt_stack);
    sigaltstack(&ss, nullptr);

    struct sigaction action {};
    action.sa_sigaction = handle_signal;
    action.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigemptyset(&action.sa_mask);
    for (int signal : {SIGSEGV, SIGILL, SIGFPE, SIGBUS, SIGTRAP}) sigaction(signal, &action, nullptr);
}

void log_backtrace(uint32_t eip, uint32_t ebp) {
    LOG_ERROR("crash", "  #0  %s", where(eip).c_str());
    for (int depth = 1; depth < 48 && on_xbox_stack(ebp); ++depth) {
        const auto* frame = reinterpret_cast<const uint32_t*>(uintptr_t{ebp});
        const uint32_t return_address = frame[1];
        if (return_address == 0) break;
        LOG_ERROR("crash", "  #%-2d %s", depth, where(return_address).c_str());
        if (frame[0] <= ebp) break;  // frames must move up the stack
        ebp = frame[0];
    }
}

void fatal_with_backtrace(const char* message, uint32_t eip, uint32_t ebp) {
    LOG_ERROR("fatal", "%s", message);
    log_backtrace(eip, ebp);
    std::_Exit(1);
}

}  // namespace loader
