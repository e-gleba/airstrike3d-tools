#include "camera.hpp"
#include "editor.hpp"
#include "game_api.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>
#include <spdlog/spdlog.h>

#include <cmath>

namespace
{

as3::engine_context*          g_ctx = nullptr;
std::vector<as3::mesh_handle> g_meshes;

// Editor (contains Scene with units)
as3::Editor g_editor;

// State
entt::entity g_camera_entity = entt::null;
float        g_time          = 0.0f;
bool         g_wireframe     = false;

// Mouse input state
bool g_mouse_was_pressed_left  = false;
bool g_mouse_was_pressed_right = false;

// Convert screen position to world position on ground plane (Y=0)
glm::vec3 screen_to_ground(float                       screen_x,
                           float                       screen_y,
                           const as3::CameraComponent& cam,
                           float                       screen_w,
                           float                       screen_h)
{
    // Build view and projection matrices
    glm::vec3 front;
    front.x =
        std::cos(glm::radians(cam.yaw)) * std::cos(glm::radians(cam.pitch));
    front.y = std::sin(glm::radians(cam.pitch));
    front.z =
        std::sin(glm::radians(cam.yaw)) * std::cos(glm::radians(cam.pitch));
    front = glm::normalize(front);

    glm::vec3 up(0.0f, 1.0f, 0.0f);
    glm::vec3 right  = glm::normalize(glm::cross(front, up));
    glm::vec3 cam_up = glm::normalize(glm::cross(right, front));

    glm::mat4 view = glm::lookAt(cam.position, cam.position + front, cam_up);
    glm::mat4 proj = glm::perspective(
        glm::radians(60.0f), screen_w / screen_h, 0.1f, 500.0f);

    // Normalize screen coords to [-1, 1]
    float ndc_x = (2.0f * screen_x / screen_w) - 1.0f;
    float ndc_y = 1.0f - (2.0f * screen_y / screen_h);

    // Unproject to get ray direction
    glm::mat4 inv_proj = glm::inverse(proj);
    glm::mat4 inv_view = glm::inverse(view);

    glm::vec4 ray_clip(ndc_x, ndc_y, -1.0f, 1.0f);
    glm::vec4 ray_eye = inv_proj * ray_clip;
    ray_eye           = glm::vec4(ray_eye.x, ray_eye.y, -1.0f, 0.0f);

    glm::vec3 ray_world = glm::normalize(glm::vec3(inv_view * ray_eye));

    // Ray-plane intersection with Y=0 plane
    if (std::abs(ray_world.y) < 0.0001f)
        return glm::vec3(0.0f); // Ray parallel to ground

    float t = -cam.position.y / ray_world.y;
    if (t < 0.0f)
        return glm::vec3(0.0f); // Intersection behind camera

    return cam.position + ray_world * t;
}

} // namespace

GAME_API bool game_init(as3::engine_context* ctx)
{
    g_ctx  = ctx;
    g_time = 0.0f;

    // Camera
    if (g_camera_entity != entt::null &&
        g_ctx->registry->valid(g_camera_entity))
        g_ctx->registry->destroy(g_camera_entity);

    g_camera_entity = g_ctx->registry->create();
    auto& cam = g_ctx->registry->emplace<as3::CameraComponent>(g_camera_entity);
    cam.position   = { 0.0f, 20.0f, 45.0f };
    cam.pitch      = -25.0f;
    cam.yaw        = -90.0f;
    cam.move_speed = 20.0f;

    // Ground grid
    g_meshes.push_back(g_ctx->renderer->create_wireframe_grid(
        150.0f, 150, { 0.12f, 0.18f, 0.12f }));

    // Initialize editor with scene system
    g_editor.init(g_ctx);
    g_editor.create_default_scene();
    g_editor.set_visible(true);

    g_ctx->renderer->set_render_mode(as3::render_mode::textured);

    spdlog::info("Game initialized");
    return true;
}

GAME_API void game_shutdown()
{
    g_editor.shutdown();

    for (auto h : g_meshes)
        if (h != as3::invalid_mesh)
            g_ctx->renderer->destroy_mesh(h);
    g_meshes.clear();

    if (g_camera_entity != entt::null &&
        g_ctx->registry->valid(g_camera_entity))
    {
        g_ctx->registry->destroy(g_camera_entity);
        g_camera_entity = entt::null;
    }

    spdlog::info("Game shutdown");
    g_ctx = nullptr;
}

GAME_API void game_update(as3::engine_context* ctx)
{
    g_time += ctx->delta_time;

    ctx->renderer->set_render_mode(g_wireframe ? as3::render_mode::wireframe
                                               : as3::render_mode::textured);

    // Handle mouse input for selection/movement (when not over ImGui)
    // Must set ImGui context first!
    if (ctx->imgui_ctx)
    {
        ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx->imgui_ctx));
        ImGuiIO& io = ImGui::GetIO();

        if (!io.WantCaptureMouse && g_camera_entity != entt::null &&
            g_ctx->registry->valid(g_camera_entity))
        {
            const auto& cam =
                g_ctx->registry->get<as3::CameraComponent>(g_camera_entity);

            bool left_pressed  = io.MouseDown[0];
            bool right_pressed = io.MouseDown[1];

            // Left click - select unit
            if (left_pressed && !g_mouse_was_pressed_left)
            {
                glm::vec3 world_pos = screen_to_ground(io.MousePos.x,
                                                       io.MousePos.y,
                                                       cam,
                                                       io.DisplaySize.x,
                                                       io.DisplaySize.y);
                g_editor.handle_click(world_pos, false);
            }

            // Right click - move command
            if (right_pressed && !g_mouse_was_pressed_right)
            {
                glm::vec3 world_pos = screen_to_ground(io.MousePos.x,
                                                       io.MousePos.y,
                                                       cam,
                                                       io.DisplaySize.x,
                                                       io.DisplaySize.y);
                g_editor.handle_click(world_pos, true);
            }

            g_mouse_was_pressed_left  = left_pressed;
            g_mouse_was_pressed_right = right_pressed;
        }
    }

    // Update scene (unit movement)
    g_editor.update(ctx->delta_time);
}

GAME_API void game_render(as3::engine_context* ctx)
{
    // Ground grid
    for (auto h : g_meshes)
        ctx->renderer->draw(h);

    // Scene units
    g_editor.scene().render();

    // Editor gizmos (bounds, axes, move targets)
    g_editor.draw_gizmos();
}

GAME_API void game_ui(as3::engine_context* ctx)
{
    ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx->imgui_ctx));

    // Main info panel
    ImGui::SetNextWindowSize(ImVec2(240, 200), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Airstrike 3D", nullptr, ImGuiWindowFlags_NoResize))
    {
        const float fps = 1.0f / ctx->delta_time;
        ImGui::Text("FPS: %.0f (%.2f ms)", fps, ctx->delta_time * 1000.0f);

        const auto stats = ctx->renderer->get_stats();
        ImGui::Separator();
        ImGui::TextColored({ 0.6f, 0.8f, 1.0f, 1.0f }, "Render:");
        ImGui::Text("Draws: %u | Tris: %u", stats.draw_calls, stats.triangles);
        ImGui::Text(
            "Models: %u | Tex: %u", stats.models_loaded, stats.textures_loaded);

        ImGui::Separator();
        ImGui::Checkbox("Wireframe", &g_wireframe);

        bool editor_visible = g_editor.is_visible();
        if (ImGui::Checkbox("Scene Editor", &editor_visible))
            g_editor.set_visible(editor_visible);

        // Camera info
        if (g_camera_entity != entt::null &&
            g_ctx->registry->valid(g_camera_entity))
        {
            const auto& cam =
                g_ctx->registry->get<as3::CameraComponent>(g_camera_entity);
            ImGui::Separator();
            ImGui::TextColored({ 0.5f, 0.5f, 0.5f, 1.0f },
                               "Cam: %.0f, %.0f, %.0f",
                               cam.position.x,
                               cam.position.y,
                               cam.position.z);
        }

        // Scene info
        ImGui::Separator();
        ImGui::TextColored({ 0.4f, 0.8f, 0.4f, 1.0f },
                           "Scene: %s",
                           g_editor.scene().config().name.c_str());
        ImGui::Text("Units: %zu", g_editor.scene().units().size());

        // Selected unit info
        auto* sel = g_editor.scene().get_selected();
        if (sel)
        {
            ImGui::TextColored(
                { 1.0f, 0.8f, 0.3f, 1.0f }, "Selected: %s", sel->name.c_str());
            if (sel->movement.type != as3::unit_type::static_object)
            {
                const char* state = "";
                switch (sel->movement.state)
                {
                    case as3::move_state::idle:
                        state = "Idle";
                        break;
                    case as3::move_state::rotating:
                        state = "Rotating";
                        break;
                    case as3::move_state::moving:
                        state = "Moving";
                        break;
                    case as3::move_state::hovering:
                        state = "Hovering";
                        break;
                }
                ImGui::Text("State: %s", state);
            }
        }
    }
    ImGui::End();

    // Scene Editor
    g_editor.draw_ui();

    // Quick help
    ImGui::SetNextWindowPos(ImVec2(10, ImGui::GetIO().DisplaySize.y - 60),
                            ImGuiCond_FirstUseEver);
    if (ImGui::Begin("##help",
                     nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                         ImGuiWindowFlags_AlwaysAutoResize |
                         ImGuiWindowFlags_NoSavedSettings))
    {
        ImGui::TextColored({ 0.5f, 0.5f, 0.5f, 1.0f },
                           "WASD/QE - fly | Mouse - look | F5 - reload");
    }
    ImGui::End();
}
