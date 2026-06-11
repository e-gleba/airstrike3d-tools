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

} // namespace sdk::logging

// ── Convenience logging functions ─────────────────────────────────────────────
//
// std::source_location::current() as a default parameter is evaluated at the
// call site, giving us correct file/line/function info without macros.
// This is the idiomatic C++20 replacement for the macro-based approach.
//
// Usage:
//   sdk::log_info("server started on port {}", port);
//   sdk::log_error("failed to open file: {}", path);
//   sdk::log_warn("deprecated API called");

namespace sdk
{

template <typename... Args>
void log_trace(std::format_string<Args...>   fmt,
               Args&&...                     args,
               std::source_location          loc = std::source_location::current())
{
    logging::detail::log_impl(logging::level::trace,
                              std::format(fmt, std::forward<Args>(args)...),
                              loc);
}

template <typename... Args>
void log_debug(std::format_string<Args...>   fmt,
               Args&&...                     args,
               std::source_location          loc = std::source_location::current())
{
    logging::detail::log_impl(logging::level::debug,
                              std::format(fmt, std::forward<Args>(args)...),
                              loc);
}

template <typename... Args>
void log_info(std::format_string<Args...>   fmt,
              Args&&...                     args,
              std::source_location          loc = std::source_location::current())
{
    logging::detail::log_impl(logging::level::info,
                              std::format(fmt, std::forward<Args>(args)...),
                              loc);
}

template <typename... Args>
void log_warn(std::format_string<Args...>   fmt,
              Args&&...                     args,
              std::source_location          loc = std::source_location::current())
{
    logging::detail::log_impl(logging::level::warn,
                              std::format(fmt, std::forward<Args>(args)...),
                              loc);
}

template <typename... Args>
void log_error(std::format_string<Args...>   fmt,
               Args&&...                     args,
               std::source_location          loc = std::source_location::current())
{
    logging::detail::log_impl(logging::level::error,
                              std::format(fmt, std::forward<Args>(args)...),
                              loc);
}

template <typename... Args>
void log_critical(std::format_string<Args...>   fmt,
                  Args&&...                     args,
                  std::source_location          loc = std::source_location::current())
{
    logging::detail::log_impl(logging::level::critical,
                              std::format(fmt, std::forward<Args>(args)...),
                              loc);
}

} // namespace sdk
