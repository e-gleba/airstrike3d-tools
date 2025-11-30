#include "engine.hpp"

#include <SDL3/SDL_main.h>
#include <spdlog/spdlog.h>

#include <cstdlib>
#include <filesystem>

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
    // Find game library next to executable
    std::filesystem::path game_path;
    if (const char* base_path = SDL_GetBasePath(); base_path)
    {
#ifdef _WIN32
        game_path = std::filesystem::path(base_path) / "game.dll";
#else
        game_path = std::filesystem::path(base_path) / "libgame.so";
#endif
    }

    const as3::engine_config config{
        .title    = "airstrike3d",
        .width    = 1280,
        .height   = 720,
        .game_lib = game_path,
    };

    as3::engine eng;

    if (!eng.init(config))
    {
        spdlog::error("Engine initialization failed");
        return EXIT_FAILURE;
    }

    eng.run();
    // shutdown() called automatically by destructor

    return EXIT_SUCCESS;
}
