#include <cstdlib>

#include "render.hpp"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

void as3::render(SDL_Renderer* renderer, [[maybe_unused]] SDL_Window* window)
{
    // Start ImGui frame
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    // ImGui windows...
    ImGui::ShowDemoWindow();
    ImGui::Begin("as3 debug");
    ImGui::Text("triangle renderer");
    ImGui::Text("fps: %.1f", ImGui::GetIO().Framerate);
    ImGui::End();

    // Render background + triangle
    SDL_SetRenderDrawColor(renderer, 20, 22, 30, 255);
    SDL_RenderClear(renderer);

    SDL_Vertex verts[3];
    // ... triangle code ...
    SDL_RenderGeometry(renderer, nullptr, verts, 3, nullptr, 0);

    // Render ImGui
    ImGui::Render();
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
    SDL_RenderPresent(renderer);
}