#include "camera.hpp"
#include "game_api.hpp"

#include <imgui.h>
#include <spdlog/spdlog.h>

#include <cmath>

namespace
{
as3::engine_context* g_ctx = nullptr;

std::vector<as3::mesh_handle> g_meshes;

// Models
as3::model_handle g_cobra      = as3::invalid_model;
as3::model_handle g_mi24       = as3::invalid_model;
as3::model_handle g_kamov      = as3::invalid_model;
as3::model_handle g_rotor_main = as3::invalid_model;
as3::model_handle g_rotor_tail = as3::invalid_model;
as3::model_handle g_tiger_tank = as3::invalid_model;

entt::entity g_camera_entity = entt::null;

float g_time = 0.0f;

constexpr float MODEL_SCALE      = 0.05f;
constexpr float ROTOR_SPEED      = 1200.0f;
constexpr float TAIL_ROTOR_SPEED = 2000.0f;

struct HeliConfig
{
    glm::vec3 pos;
    float     yaw;
    glm::vec3 rotor_offset; // main rotor offset (in model space, before scale)
    glm::vec3 tail_offset;  // tail rotor offset
    bool      has_tail;
};

// Rotor offsets - slightly above fuselage
HeliConfig g_cobra_cfg = { .pos          = { -12.0f, 3.5f, 0.0f },
                           .yaw          = 0.0f,
                           .rotor_offset = { 0.0f, 0.8f, 0.0f },
                           .tail_offset  = { 0.2f, 0.4f, -3.0f },
                           .has_tail     = true };

HeliConfig g_mi24_cfg = { .pos          = { 0.0f, 4.0f, 0.0f },
                          .yaw          = -20.0f,
                          .rotor_offset = { 0.0f, 0.9f, 0.0f },
                          .tail_offset  = { 0.2f, 0.5f, -3.5f },
                          .has_tail     = true };

HeliConfig g_kamov_cfg = {
    .pos          = { 12.0f, 3.5f, 0.0f },
    .yaw          = 20.0f,
    .rotor_offset = { 0.0f, 0.8f, 0.0f },
    .tail_offset  = { 0.0f, 0.0f, 0.0f },
    .has_tail     = false // Kamov = coaxial
};

} // namespace

GAME_API bool game_init(as3::engine_context* ctx)
{
    g_ctx  = ctx;
    g_time = 0.0f;

    if (g_camera_entity != entt::null &&
        g_ctx->registry->valid(g_camera_entity))
        g_ctx->registry->destroy(g_camera_entity);

    g_camera_entity = g_ctx->registry->create();
    auto& cam = g_ctx->registry->emplace<as3::CameraComponent>(g_camera_entity);
    cam.position   = { 0.0f, 12.0f, 35.0f };
    cam.pitch      = -15.0f;
    cam.yaw        = -90.0f;
    cam.move_speed = 15.0f;

    // Ground
    g_meshes.push_back(g_ctx->renderer->create_wireframe_grid(
        60.0f, 60, { 0.2f, 0.25f, 0.2f }));

    // Landing pads
    g_meshes.push_back(g_ctx->renderer->create_wireframe_cube(
        { -12.0f, 0.05f, 0.0f }, 2.5f, { 0.3f, 0.3f, 0.5f }));
    g_meshes.push_back(g_ctx->renderer->create_wireframe_cube(
        { 0.0f, 0.05f, 0.0f }, 3.0f, { 0.3f, 0.3f, 0.5f }));
    g_meshes.push_back(g_ctx->renderer->create_wireframe_cube(
        { 12.0f, 0.05f, 0.0f }, 2.5f, { 0.3f, 0.3f, 0.5f }));

    // Buildings
    g_meshes.push_back(g_ctx->renderer->create_wireframe_cube(
        { -20.0f, 2.0f, -18.0f }, 4.0f, { 0.4f, 0.4f, 0.4f }));
    g_meshes.push_back(g_ctx->renderer->create_wireframe_cube(
        { 20.0f, 3.0f, -18.0f }, 6.0f, { 0.4f, 0.4f, 0.4f }));
    g_meshes.push_back(g_ctx->renderer->create_wireframe_sphere(
        { -20.0f, 5.0f, -18.0f }, 1.2f, { 0.0f, 1.0f, 0.5f }, 12));

    // Load models
    g_cobra = g_ctx->renderer->load_model(
        "assets/models/helics/cobra/cobra.obj", { 0.0f, 0.9f, 0.7f });
    g_mi24 = g_ctx->renderer->load_model("assets/models/helics/mi_24/mi_24.obj",
                                         { 0.8f, 0.5f, 0.1f });
    g_kamov = g_ctx->renderer->load_model(
        "assets/models/helics/kamov/kamov.obj", { 0.2f, 0.8f, 0.3f });
    g_rotor_main = g_ctx->renderer->load_model(
        "assets/models/helics/vints/vint_c.obj", { 0.7f, 0.7f, 0.7f });
    g_rotor_tail = g_ctx->renderer->load_model(
        "assets/models/helics/vints/vint_small.obj", { 0.6f, 0.6f, 0.6f });
    g_tiger_tank = g_ctx->renderer->load_model(
        "assets/models/tanks/tiger/tiger_base.obj", { 0.6f, 0.55f, 0.3f });

    spdlog::info("Game initialized");
    return true;
}

GAME_API void game_shutdown()
{
    for (auto h : g_meshes)
        if (h != as3::invalid_mesh)
            g_ctx->renderer->destroy_mesh(h);
    g_meshes.clear();

    if (g_cobra != as3::invalid_model)
        g_ctx->renderer->unload_model(g_cobra);
    if (g_mi24 != as3::invalid_model)
        g_ctx->renderer->unload_model(g_mi24);
    if (g_kamov != as3::invalid_model)
        g_ctx->renderer->unload_model(g_kamov);
    if (g_rotor_main != as3::invalid_model)
        g_ctx->renderer->unload_model(g_rotor_main);
    if (g_rotor_tail != as3::invalid_model)
        g_ctx->renderer->unload_model(g_rotor_tail);
    if (g_tiger_tank != as3::invalid_model)
        g_ctx->renderer->unload_model(g_tiger_tank);

    g_cobra = g_mi24 = g_kamov = g_rotor_main = g_rotor_tail = g_tiger_tank =
        as3::invalid_model;

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
}

namespace
{

void draw_helicopter(as3::IRenderer*   r,
                     as3::model_handle body,
                     const HeliConfig& cfg,
                     float             time,
                     float             phase)
{
    if (body == as3::invalid_model)
        return;

    // Hover sway
    float t          = time * 0.5f + phase;
    float sway_pitch = std::sin(t * 0.7f) * 2.0f;
    float sway_roll  = std::sin(t) * 1.5f;
    float bob_y      = std::sin(t * 1.2f) * 0.05f;

    glm::vec3 pos = cfg.pos + glm::vec3(0.0f, bob_y, 0.0f);

    // Body
    r->draw_model(body,
                  { .position = pos,
                    .rotation = { sway_pitch, cfg.yaw, sway_roll },
                    .scale    = { MODEL_SCALE, MODEL_SCALE, MODEL_SCALE } });

    // Main rotor
    if (g_rotor_main != as3::invalid_model)
    {
        float     rotor_spin = std::fmod(time * ROTOR_SPEED, 360.0f);
        glm::vec3 rotor_pos  = pos + cfg.rotor_offset * MODEL_SCALE;

        r->draw_model(
            g_rotor_main,
            { .position = rotor_pos,
              .rotation = { sway_pitch, cfg.yaw + rotor_spin, sway_roll },
              .scale    = { MODEL_SCALE, MODEL_SCALE, MODEL_SCALE } });
    }

    // Tail rotor
    if (cfg.has_tail && g_rotor_tail != as3::invalid_model)
    {
        float     tail_spin = std::fmod(time * TAIL_ROTOR_SPEED, 360.0f);
        glm::vec3 tail_pos  = pos + cfg.tail_offset * MODEL_SCALE;

        r->draw_model(g_rotor_tail,
                      { .position = tail_pos,
                        .rotation = { tail_spin, cfg.yaw + 90.0f, 0.0f },
                        .scale    = { MODEL_SCALE * 0.4f,
                                      MODEL_SCALE * 0.4f,
                                      MODEL_SCALE * 0.4f } });
    }
}

} // namespace

GAME_API void game_render(as3::engine_context* ctx)
{
    for (auto h : g_meshes)
        ctx->renderer->draw(h);

    // Helicopters
    draw_helicopter(ctx->renderer, g_cobra, g_cobra_cfg, g_time, 0.0f);
    draw_helicopter(ctx->renderer, g_mi24, g_mi24_cfg, g_time, 2.0f);
    draw_helicopter(ctx->renderer, g_kamov, g_kamov_cfg, g_time, 4.0f);

    // Tank - stationary
    if (g_tiger_tank != as3::invalid_model)
    {
        ctx->renderer->draw_model(
            g_tiger_tank,
            { .position = { 8.0f, 0.0f, 12.0f },
              .rotation = { 0.0f, -45.0f, 0.0f },
              .scale    = { MODEL_SCALE, MODEL_SCALE, MODEL_SCALE } });
    }
}

GAME_API void game_ui(as3::engine_context* ctx)
{
    ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx->imgui_ctx));

    ImGui::Begin("Airstrike 3D");
    ImGui::Text("FPS: %.0f (%.2f ms)",
                1.0f / ctx->delta_time,
                ctx->delta_time * 1000.0f);
    ImGui::Separator();
    ImGui::TextColored({ 0.6f, 1.0f, 0.6f, 1.0f },
                       "WASD/QE - move | Mouse - look");
    ImGui::TextColored({ 1.0f, 1.0f, 0.6f, 1.0f },
                       "F5 - hot reload | ESC - release");
    ImGui::Separator();

    auto view = ctx->registry->view<as3::CameraComponent>();
    for (auto entity : view)
    {
        auto& cam = view.get<as3::CameraComponent>(entity);
        ImGui::Text("Pos: %.1f, %.1f, %.1f",
                    cam.position.x,
                    cam.position.y,
                    cam.position.z);
        ImGui::SliderFloat("FOV", &cam.fov, 30.0f, 120.0f);
        ImGui::SliderFloat("Speed", &cam.move_speed, 5.0f, 50.0f);
    }

    ImGui::End();
}