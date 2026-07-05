// src/proxy/sdk/core/logging_spdlog.cpp
// Logging implementation using spdlog backend.

#include "logging.hpp"

#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/sinks/msvc_sink.h>
#include <spdlog/spdlog.h>

#include <array>
#include <filesystem>
#include <memory>
#include <ranges>
#include <string>
#include <vector>

namespace sdk::logging
{

[[nodiscard]] static auto to_spdlog_level(level lvl) noexcept -> spdlog::level::level_enum
{
    switch (lvl)
    {
        case level::trace:    return spdlog::level::trace;
        case level::debug:    return spdlog::level::debug;
        case level::info:     return spdlog::level::info;
        case level::warn:     return spdlog::level::warn;
        case level::error:     return spdlog::level::err;
        case level::critical: return spdlog::level::critical;
    }
    return spdlog::level::info;
}

void init(std::string_view log_dir)
{
    namespace fs = std::filesystem;

    if (fs::path dir{ log_dir }; !fs::exists(dir))
    {
        fs::create_directories(dir);
    }

    constexpr std::string_view k_pattern{
        "[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%s:%#] %v"
    };

    // daily_file_sink_mt: produces sdk_YYYY-MM-DD.log, rotates at midnight
    auto file_sink = std::make_shared<spdlog::sinks::daily_file_sink_mt>(
        std::format("{}/sdk.log", log_dir), 0, 0);
    file_sink->set_level(spdlog::level::trace);
    file_sink->set_pattern(std::string{ k_pattern });

    auto msvc_sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
    msvc_sink->set_level(spdlog::level::debug);
    msvc_sink->set_pattern("[sdk] [%^%l%$] %v");

    auto sinks = std::array<spdlog::sink_ptr, 2>{ file_sink, msvc_sink };

    auto logger = std::make_shared<spdlog::logger>(
        "sdk", sinks.begin(), sinks.end());
    logger->set_level(spdlog::level::trace);
    logger->flush_on(spdlog::level::warn);

    spdlog::set_default_logger(std::move(logger));
}

void shutdown()
{
    spdlog::default_logger()->flush();
    spdlog::shutdown();
}

void set_level(level lvl)
{
    spdlog::set_level(to_spdlog_level(lvl));
}

namespace detail
{

void log_impl(level lvl, std::string_view msg, std::source_location loc)
{
    spdlog::log(spdlog::source_loc{ loc.file_name(),
                                    static_cast<int>(loc.line()),
                                    loc.function_name() },
                to_spdlog_level(lvl),
                "{}",
                msg);
}

} // namespace detail

} // namespace sdk::logging

// ── Wrapper function definitions ──────────────────────────────────────────────

namespace sdk
{

void log_trace(std::string_view msg, std::source_location loc)
{
    logging::detail::log_impl(logging::level::trace, msg, loc);
}

void log_debug(std::string_view msg, std::source_location loc)
{
    logging::detail::log_impl(logging::level::debug, msg, loc);
}

void log_info(std::string_view msg, std::source_location loc)
{
    logging::detail::log_impl(logging::level::info, msg, loc);
}

void log_warn(std::string_view msg, std::source_location loc)
{
    logging::detail::log_impl(logging::level::warn, msg, loc);
}

void log_error(std::string_view msg, std::source_location loc)
{
    logging::detail::log_impl(logging::level::error, msg, loc);
}

void log_critical(std::string_view msg, std::source_location loc)
{
    logging::detail::log_impl(logging::level::critical, msg, loc);
}

} // namespace sdk
