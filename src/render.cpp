
#include "render.hpp"

#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>
#include <vector>

struct camera
{
    glm::vec3 position    = { 0.0f, 2.0f, 8.0f };
    glm::vec3 front       = { 0.0f, 0.0f, -1.0f };
    glm::vec3 up          = { 0.0f, 1.0f, 0.0f };
    float     yaw         = -90.0f;
    float     pitch       = 0.0f;
    float     speed       = 5.0f;
    float     sensitivity = 0.1f;
};

struct renderable_cube
{
    glm::vec3 position;
    float     size;
    glm::vec3 color;
    float     distance_to_cam;
};

static camera cam;
static float  delta_time     = 0.016f;
static bool   mouse_captured = false;

glm::vec2 project_3d_to_2d(const glm::vec3& world_pos,
                           const glm::mat4& vp_matrix,
                           int              screen_w,
                           int              screen_h)
{
    glm::vec4 clip = vp_matrix * glm::vec4(world_pos, 1.0f);

    if (clip.w <= 0.01f)
        return { -9999.0f, -9999.0f };

    glm::vec3 ndc = glm::vec3(clip) / clip.w;

    float x = (ndc.x + 1.0f) * 0.5f * static_cast<float>(screen_w);
    float y = (1.0f - ndc.y) * 0.5f * static_cast<float>(screen_h);

    return { x, y };
}

void as3::init_renderer(SDL_Renderer* renderer, SDL_Window* window)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    SDL_SetWindowRelativeMouseMode(window, true);
    mouse_captured = true;
}

void as3::shutdown_renderer()
{
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
}

void as3::handle_event(const SDL_Event* event)
{
    if (!mouse_captured)
    {
        ImGui_ImplSDL3_ProcessEvent(event);
    }

    if (event->type == SDL_EVENT_KEY_DOWN && event->key.key == SDLK_ESCAPE)
    {
        mouse_captured     = !mouse_captured;
        SDL_Window* window = SDL_GetWindowFromID(event->key.windowID);
        SDL_SetWindowRelativeMouseMode(window, mouse_captured);
    }

    if (mouse_captured && event->type == SDL_EVENT_MOUSE_MOTION)
    {
        float xoffset =
            static_cast<float>(event->motion.xrel) * cam.sensitivity;
        float yoffset =
            static_cast<float>(-event->motion.yrel) * cam.sensitivity;

        cam.yaw += xoffset;
        cam.pitch += yoffset;

        if (cam.pitch > 89.0f)
            cam.pitch = 89.0f;
        if (cam.pitch < -89.0f)
            cam.pitch = -89.0f;

        glm::vec3 direction;
        direction.x =
            std::cos(glm::radians(cam.yaw)) * std::cos(glm::radians(cam.pitch));
        direction.y = std::sin(glm::radians(cam.pitch));
        direction.z =
            std::sin(glm::radians(cam.yaw)) * std::cos(glm::radians(cam.pitch));
        cam.front = glm::normalize(direction);
    }
}

void draw_cube(SDL_Renderer*    renderer,
               const glm::vec3& center,
               float            size,
               const glm::vec3& color,
               const glm::mat4& vp,
               int              w,
               int              h)
{
    float                  hs    = size * 0.5f;
    std::vector<glm::vec3> verts = {
        center + glm::vec3{ -hs, -hs, -hs }, center + glm::vec3{ hs, -hs, -hs },
        center + glm::vec3{ hs, hs, -hs },   center + glm::vec3{ -hs, hs, -hs },
        center + glm::vec3{ -hs, -hs, hs },  center + glm::vec3{ hs, -hs, hs },
        center + glm::vec3{ hs, hs, hs },    center + glm::vec3{ -hs, hs, hs }
    };

    std::vector<std::pair<size_t, size_t>> edges = {
        { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 }, { 4, 5 }, { 5, 6 },
        { 6, 7 }, { 7, 4 }, { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 }
    };

    SDL_SetRenderDrawColor(renderer,
                           static_cast<Uint8>(color.r * 255.0f),
                           static_cast<Uint8>(color.g * 255.0f),
                           static_cast<Uint8>(color.b * 255.0f),
                           255);

    for (const auto& [i, j] : edges)
    {
        glm::vec2 p1 = project_3d_to_2d(verts[i], vp, w, h);
        glm::vec2 p2 = project_3d_to_2d(verts[j], vp, w, h);

        if (p1.x > -9000 && p2.x > -9000)
        {
            SDL_RenderLine(renderer, p1.x, p1.y, p2.x, p2.y);
        }
    }
}

void as3::render(SDL_Renderer* renderer)
{
    static Uint64 last_time    = SDL_GetTicks();
    Uint64        current_time = SDL_GetTicks();
    delta_time = static_cast<float>(current_time - last_time) / 1000.0f;
    last_time  = current_time;

    if (mouse_captured)
    {
        const bool* keys     = SDL_GetKeyboardState(nullptr);
        float       velocity = cam.speed * delta_time;

        if (keys[SDL_SCANCODE_W])
            cam.position += cam.front * velocity;
        if (keys[SDL_SCANCODE_S])
            cam.position -= cam.front * velocity;
        if (keys[SDL_SCANCODE_A])
            cam.position -=
                glm::normalize(glm::cross(cam.front, cam.up)) * velocity;
        if (keys[SDL_SCANCODE_D])
            cam.position +=
                glm::normalize(glm::cross(cam.front, cam.up)) * velocity;
        if (keys[SDL_SCANCODE_SPACE])
            cam.position += cam.up * velocity;
        if (keys[SDL_SCANCODE_LCTRL])
            cam.position -= cam.up * velocity;
    }

    int screen_w, screen_h;
    SDL_GetRenderOutputSize(renderer, &screen_w, &screen_h);

    glm::mat4 projection = glm::perspective(glm::radians(75.0f),
                                            static_cast<float>(screen_w) /
                                                static_cast<float>(screen_h),
                                            0.1f,
                                            100.0f);
    glm::mat4 view =
        glm::lookAt(cam.position, cam.position + cam.front, cam.up);
    glm::mat4 vp = projection * view;

    SDL_SetRenderDrawColor(renderer, 20, 22, 30, 255);
    SDL_RenderClear(renderer);

    // Setup cubes and sort by distance (painter's algorithm)
    std::vector<renderable_cube> cubes = {
        { { 0, 1, 0 }, 2.0f, { 0.0f, 1.0f, 0.0f }, 0.0f },
        { { -4, 0.5f, -3 }, 1.0f, { 1.0f, 0.0f, 0.0f }, 0.0f },
        { { 4, 1.5f, 2 }, 3.0f, { 0.0f, 0.0f, 1.0f }, 0.0f },
        { { 0, 0.5f, -6 }, 1.0f, { 1.0f, 1.0f, 0.0f }, 0.0f }
    };

    // Calculate distances
    for (auto& cube : cubes)
    {
        cube.distance_to_cam = glm::distance(cam.position, cube.position);
    }

    // Sort by distance (far to near)
    std::sort(cubes.begin(),
              cubes.end(),
              [](const renderable_cube& a, const renderable_cube& b)
              { return a.distance_to_cam > b.distance_to_cam; });

    // Draw cubes from far to near
    for (const auto& cube : cubes)
    {
        draw_cube(renderer,
                  cube.position,
                  cube.size,
                  cube.color,
                  vp,
                  screen_w,
                  screen_h);
    }

    // ImGui
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("camera debug");
    ImGui::Text("pos: (%.2f, %.2f, %.2f)",
                cam.position.x,
                cam.position.y,
                cam.position.z);
    ImGui::Text("yaw: %.1f | pitch: %.1f", cam.yaw, cam.pitch);
    ImGui::SliderFloat("speed", &cam.speed, 1.0f, 20.0f);
    ImGui::Text("fps: %.1f", ImGui::GetIO().Framerate);
    if (mouse_captured)
    {
        ImGui::TextColored({ 0, 1, 0, 1 }, "mouse: captured (ESC to release)");
    }
    else
    {
        ImGui::TextColored({ 1, 0, 0, 1 }, "mouse: free (ESC to capture)");
    }
    ImGui::End();

    ImGui::Render();
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
    SDL_RenderPresent(renderer);
}
