
#include "render.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <cstdlib>

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "x11");

    if (!SDL_Init(SDL_INIT_EVENTS | SDL_INIT_VIDEO))
    {
        SDL_LogError(
            SDL_LOG_CATEGORY_APPLICATION, "init failed: %s", SDL_GetError());
        return EXIT_FAILURE;
    }

    struct sdl_guard final
    {
        ~sdl_guard() { SDL_Quit(); }
    } sdl_guard;

    SDL_Window* window =
        SDL_CreateWindow("airstrike3d",
                         800,
                         600,
                         SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!window)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "window creation failed: %s",
                     SDL_GetError());
        return EXIT_FAILURE;
    }

    SDL_GPUDevice* device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV |
                                                    SDL_GPU_SHADERFORMAT_DXIL |
                                                    SDL_GPU_SHADERFORMAT_MSL,
                                                true,
                                                nullptr);
    if (!device)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "gpu device creation failed: %s",
                     SDL_GetError());
        SDL_DestroyWindow(window);
        return EXIT_FAILURE;
    }

    if (!SDL_ClaimWindowForGPUDevice(device, window))
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "window claim failed: %s",
                     SDL_GetError());
        SDL_DestroyGPUDevice(device);
        SDL_DestroyWindow(window);
        return EXIT_FAILURE;
    }

    SDL_SetGPUSwapchainParameters(device,
                                  window,
                                  SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
                                  SDL_GPU_PRESENTMODE_VSYNC);

    as3::init_renderer(device, window);

    bool      running = true;
    SDL_Event event{};

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

        as3::render(device, window);
    }

    as3::shutdown_renderer();
    SDL_ReleaseWindowFromGPUDevice(device, window);
    SDL_DestroyGPUDevice(device);
    SDL_DestroyWindow(window);
    return EXIT_SUCCESS;
}