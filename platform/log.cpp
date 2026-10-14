#include "log.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <mutex>

namespace platform {

namespace {

std::atomic<LogLevel> g_level{LogLevel::Info};
std::mutex g_mutex;
const auto g_start = std::chrono::steady_clock::now();
std::FILE* g_file = nullptr;

const char* level_name(LogLevel level) {
    switch (level) {
        case LogLevel::Trace: return "trace";
        case LogLevel::Debug: return "debug";
        case LogLevel::Info: return "info";
        case LogLevel::Warn: return "warn";
        case LogLevel::Error: return "error";
    }
    return "?";
}

}  // namespace

void log_set_level(LogLevel level) { g_level = level; }

bool log_set_file(const char* path) {
    std::FILE* file = std::fopen(path, "a");
    if (!file) return false;
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_file) std::fclose(g_file);
    g_file = file;
    return true;
}

bool log_enabled(LogLevel level) { return level >= g_level.load(std::memory_order_relaxed); }

void vlogf(LogLevel level, const char* category, const char* fmt, va_list args) {
    if (!log_enabled(level)) return;
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - g_start)
                        .count();
    char text[2048];
    std::vsnprintf(text, sizeof(text), fmt, args);
    std::lock_guard<std::mutex> lock(g_mutex);
    for (std::FILE* out : {stderr, g_file}) {
        if (!out) continue;
        std::fprintf(out, "[%8lld.%03lld] %-5s %-8s %s\n", static_cast<long long>(ms / 1000),
                     static_cast<long long>(ms % 1000), level_name(level), category, text);
        std::fflush(out);
    }
}

void logf(LogLevel level, const char* category, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vlogf(level, category, fmt, args);
    va_end(args);
}

}  // namespace platform
