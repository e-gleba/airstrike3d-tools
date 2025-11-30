#include "game_api.hpp"
#include "camera.hpp"

#include <imgui.h>
#include <spdlog/spdlog.h>

#include <cmath>

namespace
{
as3::engine_context* g_ctx = nullptr;

// Meshes
std::vector<as3::mesh_handle> g_meshes;

// Models
as3::model_handle g_cobra = as3::invalid_model;
as3::model_handle g_mi24 = as3::invalid_model;
as3::model_handle g_tiger_tank = as3::invalid_model;
as3::model_handle g_kamov = as3::invalid_model;

// Entity handles for cleanup
entt::entity g_camera_entity = entt::null;

// Time - reset on init!
float g_time = 0.0f;
} // namespace

GAME_API bool game_init(as3::engine_context* ctx)
{
    g_ctx = ctx;
    g_time = 0.0f; // Full reset

    // Clear any existing game entities from previous session
    if (g_camera_entity != entt::null && g_ctx->registry->valid(g_camera_entity))
    {
        g_ctx->registry->destroy(g_camera_entity);
    }

    // Create camera
    g_camera_entity = g_ctx->registry->create();
    auto& cam = g_ctx->registry->emplace<as3::CameraComponent>(g_camera_entity);
    cam.position = { 0.0f, 8.0f, 25.0f };
    cam.pitch = -10.0f;
    cam.yaw = -90.0f;
    cam.move_speed = 15.0f;

    // Ground grid
    g_meshes.push_back(g_ctx->renderer->create_wireframe_grid(60.0f, 60, { 0.2f, 0.25f, 0.2f }));

    // Landing pads
    g_meshes.push_back(g_ctx->renderer->create_wireframe_cube({ -12.0f, 0.05f, 0.0f }, 2.0f, { 0.3f, 0.3f, 0.5f }));
    g_meshes.push_back(g_ctx->renderer->create_wireframe_cube({ 0.0f, 0.05f, 0.0f }, 2.5f, { 0.3f, 0.3f, 0.5f }));
    g_meshes.push_back(g_ctx->renderer->create_wireframe_cube({ 12.0f, 0.05f, 0.0f }, 2.0f, { 0.3f, 0.3f, 0.5f }));

    // Buildings
    g_meshes.push_back(g_ctx->renderer->create_wireframe_cube({ -18.0f, 1.5f, -12.0f }, 3.0f, { 0.4f, 0.4f, 0.4f }));
    g_meshes.push_back(g_ctx->renderer->create_wireframe_cube({ 18.0f, 2.5f, -15.0f }, 5.0f, { 0.4f, 0.4f, 0.4f }));

    // Radar
    g_meshes.push_back(g_ctx->renderer->create_wireframe_sphere({ -18.0f, 4.0f, -12.0f }, 1.0f, { 0.0f, 1.0f, 0.5f }, 12));

    // Load models - SCALED DOWN (0.05 = 5% of original size)
    g_cobra = g_ctx->renderer->load_model("assets/models/helics/cobra/cobra.obj", { 0.0f, 0.9f, 0.7f });
    g_mi24 = g_ctx->renderer->load_model("assets/models/helics/mi_24/mi_24.obj", { 0.8f, 0.5f, 0.1f });
    g_kamov = g_ctx->renderer->load_model("assets/models/helics/kamov/kamov.obj", { 0.2f, 0.8f, 0.3f });
    g_tiger_tank = g_ctx->renderer->load_model("assets/models/tanks/tiger/tiger_base.obj", { 0.5f, 0.6f, 0.3f });

    spdlog::info("Game initialized (time reset to 0)");
    return true;
}

GAME_API void game_shutdown()
{
    // Destroy meshes
    for (auto h : g_meshes)
    {
        if (h != as3::invalid_mesh)
            g_ctx->renderer->destroy_mesh(h);
    }
    g_meshes.clear();

    // Destroy models
    if (g_cobra != as3::invalid_model) g_ctx->renderer->unload_model(g_cobra);
    if (g_mi24 != as3::invalid_model) g_ctx->renderer->unload_model(g_mi24);
    if (g_tiger_tank != as3::invalid_model) g_ctx->renderer->unload_model(g_tiger_tank);
    if (g_kamov != as3::invalid_model) g_ctx->renderer->unload_model(g_kamov);

    g_cobra = as3::invalid_model;
    g_mi24 = as3::invalid_model;
    g_tiger_tank = as3::invalid_model;
    g_kamov = as3::invalid_model;

    // Destroy camera entity
    if (g_camera_entity != entt::null && g_ctx->registry->valid(g_camera_entity))
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
}

GAME_API void game_render(as3::engine_context* ctx)
{
    // Draw meshes
    for (auto h : g_meshes)
    {
        ctx->renderer->draw(h);
    }

    // Hover animation
    auto hover = [](float time, float phase) {
        float t = time * 0.5f + phase;
        return as3::transform{
            .position = { std::sin(t * 0.8f) * 0.05f, std::sin(t * 1.2f) * 0.03f, std::cos(t * 0.6f) * 0.03f },
            .rotation = { std::sin(t * 0.7f) * 1.5f, 0.0f, std::sin(t) * 1.0f },
            .scale = { 0.05f, 0.05f, 0.05f }  // 5% scale
        };
    };

    constexpr float MODEL_SCALE = 0.05f;

    // Cobra - left pad
    if (g_cobra != as3::invalid_model)
    {
        auto h = hover(g_time, 0.0f);
        ctx->renderer->draw_model(g_cobra, {
            .position = { -12.0f + h.position.x, 3.0f + h.position.y, 0.0f + h.position.z },
            .rotation = { h.rotation.x, 180.0f, h.rotation.z },
            .scale = { MODEL_SCALE, MODEL_SCALE, MODEL_SCALE }
        });
    }

    // MI-24 - center pad
    if (g_mi24 != as3::invalid_model)
    {
        auto h = hover(g_time, 1.5f);
        ctx->renderer->draw_model(g_mi24, {
            .position = { 0.0f + h.position.x, 4.0f + h.position.y, 0.0f + h.position.z },
            .rotation = { h.rotation.x, 160.0f, h.rotation.z },
            .scale = { MODEL_SCALE, MODEL_SCALE, MODEL_SCALE }
        });
    }

    // Kamov - right pad
    if (g_kamov != as3::invalid_model)
    {
        auto h = hover(g_time, 3.0f);
        ctx->renderer->draw_model(g_kamov, {
            .position = { 12.0f + h.position.x, 3.0f + h.position.y, 0.0f + h.position.z },
            .rotation = { h.rotation.x, 200.0f, h.rotation.z },
            .scale = { MODEL_SCALE, MODEL_SCALE, MODEL_SCALE }
        });
    }

    // Tiger tank - ground
    if (g_tiger_tank != as3::invalid_model)
    {
        ctx->renderer->draw_model(g_tiger_tank, {
            .position = { 6.0f, 0.0f, 8.0f },
            .rotation = { 0.0f, -30.0f, 0.0f },
            .scale = { MODEL_SCALE, MODEL_SCALE, MODEL_SCALE }
        });
    }
}

GAME_API void game_ui(as3::engine_context* ctx)
{
    ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx->imgui_ctx));

    ImGui::Begin("Airstrike 3D");
    ImGui::Text("FPS: %.0f (%.2f ms)", 1.0f / ctx->delta_time, ctx->delta_time * 1000.0f);
    ImGui::Separator();
    ImGui::TextColored({0.6f, 1.0f, 0.6f, 1.0f}, "WASD/QE - move | Mouse - look");
    ImGui::TextColored({1.0f, 1.0f, 0.6f, 1.0f}, "F5 - hot reload | ESC - release");
    ImGui::Separator();

    auto view = ctx->registry->view<as3::CameraComponent>();
    for (auto entity : view)
    {
        auto& cam = view.get<as3::CameraComponent>(entity);
        ImGui::Text("Pos: %.1f, %.1f, %.1f", cam.position.x, cam.position.y, cam.position.z);
        ImGui::SliderFloat("FOV", &cam.fov, 30.0f, 120.0f);
        ImGui::SliderFloat("Speed", &cam.move_speed, 5.0f, 50.0f);
    }

    ImGui::End();
}
