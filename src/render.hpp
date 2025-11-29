#include <SDL3/SDL.h>

#include <cstdlib>

void render(SDL_Renderer* renderer)
{
    SDL_SetRenderDrawColor(renderer, 20, 22, 30, 255);
    SDL_RenderClear(renderer);

    SDL_Vertex verts[3];

    verts[0].position.x = 400.0f;
    verts[0].position.y = 200.0f;
    verts[0].color.r    = 1.0f;
    verts[0].color.g    = 0.0f;
    verts[0].color.b    = 0.0f;
    verts[0].color.a    = 1.0f;

    verts[1].position.x = 300.0f;
    verts[1].position.y = 400.0f;
    verts[1].color.r    = 0.0f;
    verts[1].color.g    = 1.0f;
    verts[1].color.b    = 0.0f;
    verts[1].color.a    = 1.0f;

    verts[2].position.x = 500.0f;
    verts[2].position.y = 400.0f;
    verts[2].color.r    = 0.0f;
    verts[2].color.g    = 0.0f;
    verts[2].color.b    = 1.0f;
    verts[2].color.a    = 1.0f;

    if (!SDL_RenderGeometry(renderer, nullptr, verts, 3, nullptr, 0))
    {
        SDL_LogError(SDL_LOG_CATEGORY_RENDER,
                     "SDL_RenderGeometry failed: %s",
                     SDL_GetError());
    }

    SDL_RenderPresent(renderer);
}