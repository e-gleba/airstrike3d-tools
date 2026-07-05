#pragma once

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
// Non-template wrappers: take a pre-formatted message string.
// Callers use std::format() explicitly when interpolation is needed:
//
//   sdk::log_info("server started");
//   sdk::log_error(std::format("failed to open: {}", path));
//
// std::source_location::current() as a trailing default parameter is valid
// because there is no parameter pack — it correctly captures the call site.

namespace sdk
{

void log_trace(std::string_view     msg,
               std::source_location loc = std::source_location::current());

void log_debug(std::string_view     msg,
               std::source_location loc = std::source_location::current());

void log_info(std::string_view     msg,
              std::source_location loc = std::source_location::current());

void log_warn(std::string_view     msg,
              std::source_location loc = std::source_location::current());

void log_error(std::string_view     msg,
               std::source_location loc = std::source_location::current());

void log_critical(std::string_view     msg,
                  std::source_location loc = std::source_location::current());

} // namespace sdk
