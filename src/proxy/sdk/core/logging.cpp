#include "logging.hpp"

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/msvc_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <filesystem>
#include <format>
#include <memory>
#include <ranges>

namespace sdk::logging
{

void init(std::string_view log_dir)
{
    namespace fs = std::filesystem;

    // Ensure log directory exists — spdlog creates files but not directories
    if (fs::path dir(log_dir); !fs::exists(dir))
    {
        fs::create_directories(dir);
    }

    // Build log filename with timestamp
    auto now = std::chrono::floor<std::chrono::seconds>(
        std::chrono::system_clock::now());
    auto filename =
        std::format("{}/sdk_{:%Y-%m-%d_%H-%M-%S}.log", log_dir, now);
    auto latest = std::format("{}/latest.log", log_dir);

    // Configure and create sinks
    struct sink_config final
    {
        spdlog::sink_ptr          sink;
        spdlog::level::level_enum level;
        std::string_view          pattern;
    };

    auto configs = std::array{
        sink_config{ .sink =
                         std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                             filename, 1024uz * 1024uz * 5uz, 3),
                     .level   = spdlog::level::trace,
                     .pattern = "[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%s:%#] %v" },
        sink_config{ .sink    = std::make_shared<spdlog::sinks::msvc_sink_mt>(),
                     .level   = spdlog::level::debug,
                     .pattern = "[sdk] [%^%l%$] %v" },
        sink_config{
            .sink  = std::make_shared<spdlog::sinks::basic_file_sink_mt>(latest,
                                                                        true),
            .level = spdlog::level::trace,
            .pattern = "[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v" },
    };

    // Apply config and collect sinks
    for (auto& [sink, level, pattern] : configs)
    {
        sink->set_level(level);
        sink->set_pattern(std::string(pattern));
    }

    auto sinks = configs | std::views::transform(&sink_config::sink) |
                 std::ranges::to<std::vector>();

    // Create the multi-sink logger and set as default
    auto logger =
        std::make_shared<spdlog::logger>("sdk", sinks.begin(), sinks.end());
    logger->set_level(spdlog::level::trace);
    logger->flush_on(spdlog::level::warn);

    spdlog::flush_every(std::chrono::seconds(2));
    spdlog::set_default_logger(std::move(logger));

    spdlog::info("=== SDK Logger Initialized ===");
    spdlog::info("Log file: {}", filename);
    spdlog::info("Latest:   {}", latest);
}

void shutdown()
{
    spdlog::info("=== SDK Logger Shutting Down ===");
    spdlog::default_logger()->flush();
    spdlog::shutdown();
}

} // namespace sdk::logging