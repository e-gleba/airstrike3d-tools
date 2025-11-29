
#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>

namespace as3
{
void init_renderer(SDL_GPUDevice* device, SDL_Window* window);
void shutdown_renderer() noexcept;
void handle_event(const SDL_Event* event) noexcept;
void render(SDL_GPUDevice* device, SDL_Window* window);
} // namespace as3
