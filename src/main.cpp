#include "engine.hpp"

#include <SDL3/SDL_main.h>

#include <cstdlib>

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
    as3::engine engine;

    const as3::engine_config config{
        .title  = "airstrike3d",
        .width  = 800,
        .height = 600,
    };

    if (!engine.init(config))
    {
        return EXIT_FAILURE;
    }

    engine.run();
    engine.shutdown();

    return EXIT_SUCCESS;
}