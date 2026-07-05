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

/// @throws std::runtime_error if the log directory cannot be created or sinks fail.
void init(std::string_view log_dir = "logs");

void shutdown();
void set_level(level lvl) noexcept;

namespace detail
{
    void log_impl(level                lvl,
                  std::string_view     msg,
                  std::source_location loc);
}

} // namespace sdk::logging

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
