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
         std::format_string<Args...>   fmt,
         Args&&...                     args,
         std::source_location          loc = std::source_location::current())
{
    detail::log_impl(lvl, std::format(fmt, std::forward<Args>(args)...), loc);
}

template <typename... Args>
void trace(std::format_string<Args...> fmt,
           Args&&...                   args,
           std::source_location        loc = std::source_location::current())
{
    log(level::trace, fmt, std::forward<Args>(args)..., loc);
}

template <typename... Args>
void debug(std::format_string<Args...> fmt,
           Args&&...                   args,
           std::source_location        loc = std::source_location::current())
{
    log(level::debug, fmt, std::forward<Args>(args)..., loc);
}

template <typename... Args>
void info(std::format_string<Args...> fmt,
          Args&&...                   args,
          std::source_location        loc = std::source_location::current())
{
    log(level::info, fmt, std::forward<Args>(args)..., loc);
}

template <typename... Args>
void warn(std::format_string<Args...> fmt,
          Args&&...                   args,
          std::source_location        loc = std::source_location::current())
{
    log(level::warn, fmt, std::forward<Args>(args)..., loc);
}

template <typename... Args>
void error(std::format_string<Args...> fmt,
           Args&&...                   args,
           std::source_location        loc = std::source_location::current())
{
    log(level::error, fmt, std::forward<Args>(args)..., loc);
}

template <typename... Args>
void critical(std::format_string<Args...> fmt,
              Args&&...                   args,
              std::source_location        loc = std::source_location::current())
{
    log(level::critical, fmt, std::forward<Args>(args)..., loc);
}

} // namespace sdk::logging
