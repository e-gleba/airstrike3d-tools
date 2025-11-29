#pragma once

#include <SDL3/SDL.h>

namespace as3
{
void init_renderer(SDL_Renderer* renderer, SDL_Window* window);
void shutdown_renderer() noexcept;
void handle_event(const SDL_Event* event) noexcept;
void render(SDL_Renderer* renderer);
} // namespace as3