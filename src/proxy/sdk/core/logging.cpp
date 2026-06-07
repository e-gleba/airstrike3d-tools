#include "logging.hpp"

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/msvc_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/spdlog.h>

#include <array>
#include <chrono>
#include <filesystem>
#include <format>
#include <memory>
#include <ranges>
#include <string_view>

namespace sdk::logging
{

void init(std::string_view log_dir)
{
    namespace fs = std::filesystem;

    if (fs::path dir{ log_dir }; !fs::exists(dir))
    {
        fs::create_directories(dir);
    }

    auto now{ std::chrono::floor<std::chrono::seconds>(
        std::chrono::system_clock::now()) };
    auto filename{ std::format(
        "{}/sdk_{:%Y-%m-%d_%H-%M-%S}.log", log_dir, now) };
    auto latest{ std::format("{}/latest.log", log_dir) };

    struct sink_config final
    {
        spdlog::sink_ptr          sink;
        spdlog::level::level_enum level;
        std::string_view          pattern;
    };

    constexpr std::size_t      k_max_file_size{ 1024uz * 1024uz * 5uz };
    constexpr std::size_t      k_max_files{ 3 };
    constexpr std::string_view k_full_pattern{
        "[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%s:%#] %v"
    };
    constexpr std::string_view k_msvc_pattern{ "[sdk] [%^%l%$] %v" };

    auto configs{ std::array{
        sink_config{ .sink =
                         std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                             filename, k_max_file_size, k_max_files),
                     .level   = spdlog::level::trace,
                     .pattern = k_full_pattern },
        sink_config{ .sink    = std::make_shared<spdlog::sinks::msvc_sink_mt>(),
                     .level   = spdlog::level::debug,
                     .pattern = k_msvc_pattern },
        sink_config{
            .sink  = std::make_shared<spdlog::sinks::basic_file_sink_mt>(latest,
                                                                         true),
            .level = spdlog::level::trace,
            .pattern = k_full_pattern },
    } };

    for (auto& [sink, level, pattern] : configs)
    {
        sink->set_level(level);
        sink->set_pattern(std::string{ pattern });
    }

    auto sinks{ configs | std::views::transform(&sink_config::sink) |
                std::ranges::to<std::vector>() };

    auto logger{ std::make_shared<spdlog::logger>(
        "sdk", sinks.begin(), sinks.end()) };
    logger->set_level(spdlog::level::trace);
    logger->flush_on(spdlog::level::warn);

    spdlog::flush_every(std::chrono::seconds{ 2 });
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