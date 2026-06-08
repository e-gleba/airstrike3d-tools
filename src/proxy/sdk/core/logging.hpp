#pragma once

#include <format>
#include <source_location>
#include <string_view>

namespace sdk::logging
{

enum class level
{
    trace,
    debug,
    info,
    warn,
    error,
    critical
};

void init(std::string_view log_dir = "logs");
void shutdown();
void set_level(level lvl);

namespace detail
{
    void log_impl(level                lvl,
                  std::string_view     msg,
                  std::source_location loc);
}

template <typename... Args>
void log(level                         lvl,
         std::source_location          loc,
         std::format_string<Args...>   fmt,
         Args&&...                     args)
{
    detail::log_impl(lvl, std::format(fmt, std::forward<Args>(args)...), loc);
}

} // namespace sdk::logging

// ── Convenience macros ────────────────────────────────────────────────────────
// std::source_location::current() MUST be evaluated at the call site.
// Trailing default parameter after a variadic pack is not deducible,
// so macros are the only correct approach (same reason spdlog uses SPDLOG_*).
// NOLINTBEGIN(cppcoreguidelines-macro-usage)

#define SDK_LOG_(lvl_, ...)                                                    \
    ::sdk::logging::log(lvl_, std::source_location::current()                  \
                        __VA_OPT__(,) __VA_ARGS__)

#define SDK_TRACE(...)    SDK_LOG_(::sdk::logging::level::trace __VA_OPT__(,) __VA_ARGS__)
#define SDK_DEBUG(...)    SDK_LOG_(::sdk::logging::level::debug __VA_OPT__(,) __VA_ARGS__)
#define SDK_INFO(...)     SDK_LOG_(::sdk::logging::level::info  __VA_OPT__(,) __VA_ARGS__)
#define SDK_WARN(...)     SDK_LOG_(::sdk::logging::level::warn  __VA_OPT__(,) __VA_ARGS__)
#define SDK_ERROR(...)    SDK_LOG_(::sdk::logging::level::error __VA_OPT__(,) __VA_ARGS__)
#define SDK_CRITICAL(...) SDK_LOG_(::sdk::logging::level::critical __VA_OPT__(,) __VA_ARGS__)

// NOLINTEND(cppcoreguidelines-macro-usage)
