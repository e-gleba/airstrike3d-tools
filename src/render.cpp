#include "render.hpp"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

void as3::init_renderer(SDL_Renderer* renderer, SDL_Window* window)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);
}

void as3::shutdown_renderer()
{
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
}

void as3::handle_event(const SDL_Event* event)
{
    ImGui_ImplSDL3_ProcessEvent(event);
}

void as3::render(SDL_Renderer* renderer)
{
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    ImGui::ShowDemoWindow();
    ImGui::Begin("as3 debug");
    ImGui::Text("triangle renderer");
    ImGui::Text("fps: %.1f", ImGui::GetIO().Framerate);
    ImGui::End();

    SDL_SetRenderDrawColor(renderer, 20, 22, 30, 255);
    SDL_RenderClear(renderer);

    SDL_Vertex verts[3];
    verts[0].position = { 400.0f, 200.0f };
    verts[0].color    = { 1.0f, 0.0f, 0.0f, 1.0f };
    verts[1].position = { 300.0f, 400.0f };
    verts[1].color    = { 0.0f, 1.0f, 0.0f, 1.0f };
    verts[2].position = { 500.0f, 400.0f };
    verts[2].color    = { 0.0f, 0.0f, 1.0f, 1.0f };

    SDL_RenderGeometry(renderer, nullptr, verts, 3, nullptr, 0);

    ImGui::Render();
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
    SDL_RenderPresent(renderer);
}
