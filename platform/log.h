#pragma once

#include <cstdarg>

#include "compiler.h"

namespace platform {

enum class LogLevel { Trace, Debug, Info, Warn, Error };

void log_set_level(LogLevel level);
// Also writes every line to `path` (appending). False if it cannot be opened.
bool log_set_file(const char* path);
bool log_enabled(LogLevel level);
void logf(LogLevel level, const char* category, const char* fmt, ...) PLATFORM_PRINTF(3, 4);
void vlogf(LogLevel level, const char* category, const char* fmt, va_list args);

}  // namespace platform

#define PLATFORM_LOG(level, category, ...)                                  \
    do {                                                                    \
        if (::platform::log_enabled(level))                                 \
            ::platform::logf(level, category, __VA_ARGS__);                 \
    } while (0)

#define LOG_TRACE(category, ...) PLATFORM_LOG(::platform::LogLevel::Trace, category, __VA_ARGS__)
#define LOG_DEBUG(category, ...) PLATFORM_LOG(::platform::LogLevel::Debug, category, __VA_ARGS__)
#define LOG_INFO(category, ...) PLATFORM_LOG(::platform::LogLevel::Info, category, __VA_ARGS__)
#define LOG_WARN(category, ...) PLATFORM_LOG(::platform::LogLevel::Warn, category, __VA_ARGS__)
#define LOG_ERROR(category, ...) PLATFORM_LOG(::platform::LogLevel::Error, category, __VA_ARGS__)
