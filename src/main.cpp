#include "debug_ui.hpp"
#include "engine.hpp"

#include <SDL3/SDL_main.h>

#include <cstdlib>

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
    as3::engine   eng;
    as3::debug_ui ui;

    const as3::engine_config config{
        .title  = "airstrike3d",
        .width  = 800,
        .height = 600,
    };

    if (!eng.init(config))
    {
        return EXIT_FAILURE;
    }

    eng.set_ui_callback([&eng, &ui]() { ui.draw(eng); });

    eng.run();
    eng.shutdown();

    return EXIT_SUCCESS;
}
