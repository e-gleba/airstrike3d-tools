#include "engine.hpp"

#include <SDL3/SDL_main.h>

#include <cstdlib>
#include <filesystem>

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
    as3::engine eng;

    // Find game library next to executable
    const char* base_path = SDL_GetBasePath();
    std::filesystem::path game_path;
    if (base_path)
    {
#ifdef _WIN32
        game_path = std::filesystem::path(base_path) / "game.dll";
#else
        game_path = std::filesystem::path(base_path) / "libgame.so";
#endif
    }

    const as3::engine_config config{
        .title    = "airstrike3d",
        .width    = 800,
        .height   = 600,
        .game_lib = game_path.empty() ? nullptr : game_path.c_str(),
    };

    if (!eng.init(config))
    {
        return EXIT_FAILURE;
    }

    eng.run();
    eng.shutdown();

    return EXIT_SUCCESS;
}

