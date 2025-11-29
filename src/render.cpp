#include "render.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>
#include <ranges>

struct camera final
{
    glm::vec3 position    = { 0.0f, 2.0f, 8.0f };
    glm::vec3 front       = { 0.0f, 0.0f, -1.0f };
    glm::vec3 up          = { 0.0f, 1.0f, 0.0f };
    float     yaw         = -90.0f;
    float     pitch       = 0.0f;
    float     speed       = 5.0f;
    float     sensitivity = 0.1f;
};

struct renderable_cube final
{
    glm::vec3 position;
    float     size;
    glm::vec3 color;
    float     distance_to_cam;

    void update_distance(const glm::vec3& cam_pos) noexcept
    {
        distance_to_cam = glm::distance(cam_pos, position);
    }
};

namespace
{
constexpr float k_min_clip_distance = 0.01f;
constexpr float k_offscreen_coord   = -9999.0f;
constexpr float k_max_pitch         = 89.0f;
constexpr float k_min_pitch         = -89.0f;
constexpr float k_fov_degrees       = 75.0f;
constexpr float k_near_plane        = 0.1f;
constexpr float k_far_plane         = 100.0f;

camera cam;
float  delta_time     = 0.016f;
bool   mouse_captured = false;

// cube vertices relative to center
constexpr std::array<glm::vec3, 8> k_cube_offsets = { {
    { -1.0f, -1.0f, -1.0f },
    { 1.0f, -1.0f, -1.0f },
    { 1.0f, 1.0f, -1.0f },
    { -1.0f, 1.0f, -1.0f },
    { -1.0f, -1.0f, 1.0f },
    { 1.0f, -1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f },
    { -1.0f, 1.0f, 1.0f },
} };

// cube edges as index pairs
constexpr std::array<std::pair<size_t, size_t>, 12> k_cube_edges = { {
    { 0, 1 },
    { 1, 2 },
    { 2, 3 },
    { 3, 0 },
    { 4, 5 },
    { 5, 6 },
    { 6, 7 },
    { 7, 4 },
    { 0, 4 },
    { 1, 5 },
    { 2, 6 },
    { 3, 7 },
} };

[[nodiscard]] glm::vec2 project_3d_to_2d(const glm::vec3& world_pos,
                                         const glm::mat4& vp_matrix,
                                         int              screen_w,
                                         int              screen_h) noexcept
{
    const glm::vec4 clip = vp_matrix * glm::vec4(world_pos, 1.0f);

    if (clip.w <= k_min_clip_distance)
        return { k_offscreen_coord, k_offscreen_coord };

    const glm::vec3 ndc = glm::vec3(clip) / clip.w;

    const float x = (ndc.x + 1.0f) * 0.5f * static_cast<float>(screen_w);
    const float y = (1.0f - ndc.y) * 0.5f * static_cast<float>(screen_h);

    return { x, y };
}

[[nodiscard]] constexpr float clamp_pitch(float pitch) noexcept
{
    return std::clamp(pitch, k_min_pitch, k_max_pitch);
}

void update_camera_direction(camera& c) noexcept
{
    glm::vec3 direction;
    direction.x =
        std::cos(glm::radians(c.yaw)) * std::cos(glm::radians(c.pitch));
    direction.y = std::sin(glm::radians(c.pitch));
    direction.z =
        std::sin(glm::radians(c.yaw)) * std::cos(glm::radians(c.pitch));
    c.front = glm::normalize(direction);
}

void handle_mouse_motion(camera& c,
                         float   xrel,
                         float   yrel,
                         float   sensitivity) noexcept
{
    c.yaw += xrel * sensitivity;
    c.pitch = clamp_pitch(c.pitch + (-yrel) * sensitivity);
    update_camera_direction(c);
}

void draw_cube(SDL_Renderer*    renderer,
               const glm::vec3& center,
               float            size,
               const glm::vec3& color,
               const glm::mat4& vp,
               int              w,
               int              h) noexcept
{
    const float half_size = size * 0.5f;

    std::array<glm::vec3, 8> verts;
    std::ranges::transform(k_cube_offsets,
                           verts.begin(),
                           [&](const glm::vec3& offset) noexcept
                           { return center + offset * half_size; });

    SDL_SetRenderDrawColor(renderer,
                           static_cast<Uint8>(color.r * 255.0f),
                           static_cast<Uint8>(color.g * 255.0f),
                           static_cast<Uint8>(color.b * 255.0f),
                           255);

    for (const auto& [i, j] : k_cube_edges)
    {
        const glm::vec2 p1 = project_3d_to_2d(verts[i], vp, w, h);
        const glm::vec2 p2 = project_3d_to_2d(verts[j], vp, w, h);

        if (p1.x > -9000.0f && p2.x > -9000.0f) [[likely]]
        {
            SDL_RenderLine(renderer, p1.x, p1.y, p2.x, p2.y);
        }
    }
}

void update_camera_position(camera&     c,
                            const bool* keys,
                            float       velocity) noexcept
{
    if (keys[SDL_SCANCODE_W])
        c.position += c.front * velocity;
    if (keys[SDL_SCANCODE_S])
        c.position -= c.front * velocity;

    const glm::vec3 right = glm::normalize(glm::cross(c.front, c.up));
    if (keys[SDL_SCANCODE_A])
        c.position -= right * velocity;
    if (keys[SDL_SCANCODE_D])
        c.position += right * velocity;

    if (keys[SDL_SCANCODE_SPACE])
        c.position += c.up * velocity;
    if (keys[SDL_SCANCODE_LCTRL])
        c.position -= c.up * velocity;
}
} // namespace

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

void as3::shutdown_renderer() noexcept
{
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
}

void as3::handle_event(const SDL_Event* event) noexcept
{
    if (!mouse_captured)
    {
        ImGui_ImplSDL3_ProcessEvent(event);
    }

    if (event->type == SDL_EVENT_KEY_DOWN && event->key.key == SDLK_ESCAPE)
        [[unlikely]]
    {
        mouse_captured     = !mouse_captured;
        SDL_Window* window = SDL_GetWindowFromID(event->key.windowID);
        SDL_SetWindowRelativeMouseMode(window, mouse_captured);
    }

    if (mouse_captured && event->type == SDL_EVENT_MOUSE_MOTION) [[likely]]
    {
        handle_mouse_motion(cam,
                            static_cast<float>(event->motion.xrel),
                            static_cast<float>(event->motion.yrel),
                            cam.sensitivity);
    }
}

void as3::render(SDL_Renderer* renderer)
{
    static Uint64 last_time    = SDL_GetTicks();
    const Uint64  current_time = SDL_GetTicks();
    delta_time = static_cast<float>(current_time - last_time) / 1000.0f;
    last_time  = current_time;

    if (mouse_captured)
    {
        const bool* keys     = SDL_GetKeyboardState(nullptr);
        const float velocity = cam.speed * delta_time;
        update_camera_position(cam, keys, velocity);
    }

    int screen_w{}, screen_h{};
    SDL_GetRenderOutputSize(renderer, &screen_w, &screen_h);

    const float aspect_ratio =
        static_cast<float>(screen_w) / static_cast<float>(screen_h);
    const glm::mat4 projection = glm::perspective(
        glm::radians(k_fov_degrees), aspect_ratio, k_near_plane, k_far_plane);
    const glm::mat4 view =
        glm::lookAt(cam.position, cam.position + cam.front, cam.up);
    const glm::mat4 vp = projection * view;

    SDL_SetRenderDrawColor(renderer, 20, 22, 30, 255);
    SDL_RenderClear(renderer);

    // setup cubes and sort by distance (painter's algorithm)
    std::array<renderable_cube, 4> cubes = { {
        { { 0.0f, 1.0f, 0.0f }, 2.0f, { 0.0f, 1.0f, 0.0f }, 0.0f },
        { { -4.0f, 0.5f, -3.0f }, 1.0f, { 1.0f, 0.0f, 0.0f }, 0.0f },
        { { 4.0f, 1.5f, 2.0f }, 3.0f, { 0.0f, 0.0f, 1.0f }, 0.0f },
        { { 0.0f, 0.5f, -6.0f }, 1.0f, { 1.0f, 1.0f, 0.0f }, 0.0f },
    } };

    // calculate distances and sort (far to near)
    std::ranges::for_each(cubes,
                          [&](renderable_cube& cube) noexcept
                          { cube.update_distance(cam.position); });

    std::ranges::sort(
        cubes,
        [](const renderable_cube& a, const renderable_cube& b) noexcept
        { return a.distance_to_cam > b.distance_to_cam; });

    // draw cubes from far to near
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

    // imgui
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
        ImGui::TextColored({ 0.0f, 1.0f, 0.0f, 1.0f },
                           "mouse: captured (ESC to release)");
    }
    else
    {
        ImGui::TextColored({ 1.0f, 0.0f, 0.0f, 1.0f },
                           "mouse: free (ESC to capture)");
    }
    ImGui::End();

    ImGui::Render();
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
    SDL_RenderPresent(renderer);
}
