#include "engine.hpp"

#include <core-api/platform.hpp>
#include <core-api/window.hpp>

#include <SDL3/SDL_main.h>
#include <spdlog/spdlog.h>

#include <cstdlib>
#include <filesystem>

namespace
{

[[nodiscard]] std::filesystem::path get_game_library_path()
{
    const char* base_path = SDL_GetBasePath();
    if (base_path == nullptr)
    {
        return {};
    }

    return std::filesystem::path(base_path) /
           euengine::platform::game_library_name();
}

} // namespace

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
    using namespace euengine;

    constexpr window_settings window {
        .title     = "euengine",
        .width     = 1280,
        .height    = 720,
        .mode      = window_mode::windowed,
        .vsync     = vsync_mode::enabled,
        .resizable = true,
        .high_dpi  = true,
    };

    const engine_config config {
        .window   = window,
        .game_lib = get_game_library_path(),
    };

    engine eng {};

    if (!eng.init(config))
    {
        spdlog::error("Engine initialization failed");
        return EXIT_FAILURE;
    }

    eng.run();

    return EXIT_SUCCESS;
}