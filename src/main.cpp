#include "render.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <cstdlib>

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "x11");

    if (!SDL_Init(SDL_INIT_EVENTS | SDL_INIT_VIDEO))
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_Init failed: %s",
                     SDL_GetError());
        return EXIT_FAILURE;
    }

    struct sdl_guard final
    {
        ~sdl_guard() { SDL_Quit(); }
    } sdl_guard;

    SDL_Window* window =
        SDL_CreateWindow("Hello World", 800, 600, SDL_WINDOW_RESIZABLE);
    if (!window)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_CreateWindow failed: %s",
                     SDL_GetError());
        return EXIT_FAILURE;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_CreateRenderer failed: %s",
                     SDL_GetError());
        SDL_DestroyWindow(window);
        return EXIT_FAILURE;
    }

    as3::init_renderer(renderer, window);

    bool               running = true;
    SDL_Event          event{};
    constexpr uint32_t max_fps = 60;

    while (running)
    {
        while (SDL_PollEvent(&event))
        {
            as3::handle_event(&event);
            if (event.type == SDL_EVENT_QUIT)
            {
                running = false;
            }
        }

        as3::render(renderer);
        SDL_Delay(1000 / max_fps);
    }

    as3::shutdown_renderer();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    return EXIT_SUCCESS;
}