#include "camera.hpp"
#include "game_api.hpp"

#include <imgui.h>
#include <spdlog/spdlog.h>

#include <cmath>
#include <numbers>

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

// Audio
std::vector<as3::music_handle> g_music_tracks;
std::vector<std::string> g_music_names;
int g_current_track = -1;

entt::entity g_camera_entity = entt::null;
float g_time = 0.0f;
bool g_wireframe = false;

constexpr float MODEL_SCALE      = 0.04f;
constexpr float ROTOR_SPEED      = 900.0f;
constexpr float TAIL_ROTOR_SPEED = 1500.0f;

struct HeliConfig
{
    glm::vec3 base_pos;
    float     base_yaw;
    float     rotor_height;
    float     tail_length;
    float     tail_height;
    float     tail_side;
    bool      has_tail;
    float     hover_freq;
    float     hover_amplitude;
};

HeliConfig g_cobra_cfg = {
    .base_pos        = { -15.0f, 4.0f, 0.0f },
    .base_yaw        = 15.0f,
    .rotor_height    = 1.8f,
    .tail_length     = -3.5f,
    .tail_height     = 0.5f,
    .tail_side       = 0.15f,
    .has_tail        = true,
    .hover_freq      = 0.8f,
    .hover_amplitude = 1.0f
};

HeliConfig g_mi24_cfg = {
    .base_pos        = { 0.0f, 5.0f, 0.0f },
    .base_yaw        = -10.0f,
    .rotor_height    = 2.2f,
    .tail_length     = -4.0f,
    .tail_height     = 0.8f,
    .tail_side       = 0.2f,
    .has_tail        = true,
    .hover_freq      = 0.6f,
    .hover_amplitude = 0.8f
};

HeliConfig g_kamov_cfg = {
    .base_pos        = { 15.0f, 4.5f, 0.0f },
    .base_yaw        = -20.0f,
    .rotor_height    = 2.0f,
    .tail_length     = 0.0f,
    .tail_height     = 0.0f,
    .tail_side       = 0.0f,
    .has_tail        = false,
    .hover_freq      = 0.7f,
    .hover_amplitude = 0.9f
};

struct HoverState
{
    glm::vec3 position;
    float pitch;
    float yaw;
    float roll;
};

HoverState calculate_hover(const HeliConfig& cfg, float time, float phase)
{
    const float t = time * cfg.hover_freq + phase;
    const float amp = cfg.hover_amplitude;
    
    const float f1 = t * 1.0f;
    const float f2 = t * 1.7f;
    const float f3 = t * 0.6f;
    const float f4 = t * 2.3f;
    
    HoverState state;
    state.position = cfg.base_pos + glm::vec3(
        std::sin(f1) * 0.15f * amp + std::sin(f3) * 0.08f * amp,
        std::sin(f2) * 0.12f * amp + std::cos(f4) * 0.06f * amp + 0.05f,
        std::cos(f1 * 0.8f) * 0.1f * amp
    );
    
    state.pitch = std::sin(f1 * 0.9f) * 4.0f * amp + std::sin(f3) * 2.0f * amp;
    state.roll  = std::sin(f2 * 0.7f) * 3.5f * amp + std::cos(f4 * 0.5f) * 1.5f * amp;
    state.yaw   = cfg.base_yaw + std::sin(f3 * 0.4f) * 2.0f * amp;
    
    return state;
}

void draw_helicopter(as3::IRenderer* r, as3::model_handle body, const HeliConfig& cfg, float time, float phase)
{
    if (body == as3::invalid_model) return;

    const auto hover = calculate_hover(cfg, time, phase);

    // Get helicopter bounds to position rotor correctly on top
    const auto heli_bounds = r->get_bounds(body);
    const float heli_top = heli_bounds.max.y * MODEL_SCALE;  // Top of helicopter in world scale

    r->draw_model(body, {
        .position = hover.position,
        .rotation = { hover.pitch, hover.yaw, hover.roll },
        .scale    = { MODEL_SCALE, MODEL_SCALE, MODEL_SCALE }
    });

    if (g_rotor_main != as3::invalid_model)
    {
        const float rotor_spin = std::fmod(time * ROTOR_SPEED, 360.0f);
        // Position rotor at top of helicopter bounds
        const glm::vec3 rotor_pos = hover.position + glm::vec3(0.0f, heli_top, 0.0f);
        
        r->draw_model(g_rotor_main, {
            .position = rotor_pos,
            .rotation = { hover.pitch * 0.3f, hover.yaw + rotor_spin, hover.roll * 0.3f },
            .scale    = { MODEL_SCALE * 1.2f, MODEL_SCALE * 1.2f, MODEL_SCALE * 1.2f }
        });
    }

    if (cfg.has_tail && g_rotor_tail != as3::invalid_model)
    {
        const float tail_spin = std::fmod(time * TAIL_ROTOR_SPEED, 360.0f);
        
        // Use bounds to position tail rotor at back of helicopter
        const float tail_back = heli_bounds.min.z * MODEL_SCALE;  // Back of helicopter
        
        const float yaw_rad = glm::radians(hover.yaw);
        const float cos_yaw = std::cos(yaw_rad);
        const float sin_yaw = std::sin(yaw_rad);
        
        const glm::vec3 local_tail = { cfg.tail_side, cfg.tail_height, tail_back };
        const glm::vec3 world_tail = {
            local_tail.x * cos_yaw - local_tail.z * sin_yaw,
            local_tail.y,
            local_tail.x * sin_yaw + local_tail.z * cos_yaw
        };
        
        const glm::vec3 tail_pos = hover.position + world_tail;

        r->draw_model(g_rotor_tail, {
            .position = tail_pos,
            .rotation = { tail_spin, hover.yaw + 90.0f, 0.0f },
            .scale    = { MODEL_SCALE * 0.6f, MODEL_SCALE * 0.6f, MODEL_SCALE * 0.6f }
        });
    }
}

} // namespace

GAME_API bool game_init(as3::engine_context* ctx)
{
    g_ctx  = ctx;
    g_time = 0.0f;

    if (g_camera_entity != entt::null && g_ctx->registry->valid(g_camera_entity))
    {
        g_ctx->registry->destroy(g_camera_entity);
    }

    g_camera_entity = g_ctx->registry->create();
    auto& cam = g_ctx->registry->emplace<as3::CameraComponent>(g_camera_entity);
    cam.position   = { 0.0f, 8.0f, 28.0f };
    cam.pitch      = -10.0f;
    cam.yaw        = -90.0f;
    cam.move_speed = 12.0f;

    // Ground grid
    g_meshes.push_back(g_ctx->renderer->create_wireframe_grid(100.0f, 100, { 0.12f, 0.16f, 0.12f }));

    // Helipads
    g_meshes.push_back(g_ctx->renderer->create_wireframe_cube({ -15.0f, 0.02f, 0.0f }, 4.0f, { 0.2f, 0.2f, 0.5f }));
    g_meshes.push_back(g_ctx->renderer->create_wireframe_cube({ 0.0f, 0.02f, 0.0f }, 5.0f, { 0.2f, 0.2f, 0.5f }));
    g_meshes.push_back(g_ctx->renderer->create_wireframe_cube({ 15.0f, 0.02f, 0.0f }, 4.0f, { 0.2f, 0.2f, 0.5f }));

    // Buildings
    g_meshes.push_back(g_ctx->renderer->create_wireframe_cube({ -30.0f, 3.0f, -25.0f }, 6.0f, { 0.3f, 0.3f, 0.3f }));
    g_meshes.push_back(g_ctx->renderer->create_wireframe_cube({ 30.0f, 4.0f, -28.0f }, 8.0f, { 0.3f, 0.3f, 0.3f }));
    g_meshes.push_back(g_ctx->renderer->create_wireframe_cube({ 0.0f, 2.0f, -30.0f }, 4.0f, { 0.3f, 0.3f, 0.3f }));
    
    // Control tower
    g_meshes.push_back(g_ctx->renderer->create_wireframe_cube({ -30.0f, 8.0f, -25.0f }, 3.0f, { 0.35f, 0.35f, 0.35f }));
    g_meshes.push_back(g_ctx->renderer->create_wireframe_sphere({ -30.0f, 11.0f, -25.0f }, 2.0f, { 0.0f, 0.9f, 0.4f }, 16));
    
    // Fuel tanks
    g_meshes.push_back(g_ctx->renderer->create_wireframe_sphere({ 25.0f, 2.0f, 20.0f }, 2.0f, { 0.6f, 0.3f, 0.1f }, 12));
    g_meshes.push_back(g_ctx->renderer->create_wireframe_sphere({ 30.0f, 2.0f, 18.0f }, 2.0f, { 0.6f, 0.3f, 0.1f }, 12));

    // Load helicopter models (these will auto-load textures if available)
    g_cobra = g_ctx->renderer->load_model("assets/models/helics/cobra/cobra.obj", { 0.0f, 0.85f, 0.75f });
    g_mi24 = g_ctx->renderer->load_model("assets/models/helics/mi_24/mi_24.obj", { 0.95f, 0.55f, 0.1f });
    g_kamov = g_ctx->renderer->load_model("assets/models/helics/kamov/kamov.obj", { 0.15f, 0.75f, 0.2f });
    
    g_rotor_main = g_ctx->renderer->load_model("assets/models/helics/vints/vint_c.obj", { 0.85f, 0.85f, 0.85f });
    g_rotor_tail = g_ctx->renderer->load_model("assets/models/helics/vints/vint_small.obj", { 0.75f, 0.75f, 0.75f });
    
    g_tiger_tank = g_ctx->renderer->load_model("assets/models/tanks/tiger/tiger_base.obj", { 0.5f, 0.45f, 0.25f });

    // Set textured mode by default
    g_ctx->renderer->set_render_mode(as3::render_mode::textured);
    
    // Load music
    if (g_ctx->audio)
    {
        g_music_names = g_ctx->audio->list_music_files();
        for (const auto& name : g_music_names)
        {
            auto h = g_ctx->audio->load_music("assets/music/" + name);
            g_music_tracks.push_back(h);
        }
        
        // Start playing first track if available
        if (!g_music_tracks.empty() && g_music_tracks[0] != as3::invalid_music)
        {
            g_current_track = 0;
            g_ctx->audio->play_music(g_music_tracks[0]);
        }
    }

    spdlog::info("Game initialized - airfield scene with music");
    return true;
}

GAME_API void game_shutdown()
{
    for (auto h : g_meshes)
    {
        if (h != as3::invalid_mesh)
            g_ctx->renderer->destroy_mesh(h);
    }
    g_meshes.clear();

    auto unload = [](as3::model_handle& m) {
        if (m != as3::invalid_model) {
            g_ctx->renderer->unload_model(m);
            m = as3::invalid_model;
        }
    };
    
    unload(g_cobra);
    unload(g_mi24);
    unload(g_kamov);
    unload(g_rotor_main);
    unload(g_rotor_tail);
    unload(g_tiger_tank);

    if (g_camera_entity != entt::null && g_ctx->registry->valid(g_camera_entity))
    {
        g_ctx->registry->destroy(g_camera_entity);
        g_camera_entity = entt::null;
    }
    
    // Cleanup audio
    if (g_ctx->audio)
    {
        g_ctx->audio->stop_music();
        for (auto h : g_music_tracks)
        {
            if (h != as3::invalid_music)
                g_ctx->audio->unload_music(h);
        }
    }
    g_music_tracks.clear();
    g_music_names.clear();
    g_current_track = -1;

    spdlog::info("Game shutdown");
    g_ctx = nullptr;
}

GAME_API void game_update(as3::engine_context* ctx)
{
    g_time += ctx->delta_time;
    
    // Update render mode based on toggle
    ctx->renderer->set_render_mode(g_wireframe ? as3::render_mode::wireframe : as3::render_mode::textured);
}

GAME_API void game_render(as3::engine_context* ctx)
{
    // Draw wireframes first
    for (auto h : g_meshes)
    {
        ctx->renderer->draw(h);
    }

    // Draw textured models
    draw_helicopter(ctx->renderer, g_cobra, g_cobra_cfg, g_time, 0.0f);
    draw_helicopter(ctx->renderer, g_mi24, g_mi24_cfg, g_time, 2.1f);
    draw_helicopter(ctx->renderer, g_kamov, g_kamov_cfg, g_time, 4.7f);

    // Tank
    if (g_tiger_tank != as3::invalid_model)
    {
        ctx->renderer->draw_model(g_tiger_tank, {
            .position = { 20.0f, 0.0f, 12.0f },
            .rotation = { 0.0f, -120.0f, 0.0f },
            .scale    = { MODEL_SCALE, MODEL_SCALE, MODEL_SCALE }
        });
    }
}

GAME_API void game_ui(as3::engine_context* ctx)
{
    ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx->imgui_ctx));

    ImGui::Begin("Airstrike 3D");
    
    const float fps = 1.0f / ctx->delta_time;
    ImGui::Text("FPS: %.0f (%.2f ms)", fps, ctx->delta_time * 1000.0f);
    ImGui::Separator();
    
    // Wireframe toggle
    ImGui::Checkbox("Wireframe Mode", &g_wireframe);
    ImGui::Separator();
    
    ImGui::TextColored({ 0.4f, 1.0f, 0.4f, 1.0f }, "Controls:");
    ImGui::BulletText("WASD/QE - fly camera");
    ImGui::BulletText("Mouse - look (click to capture)");
    ImGui::BulletText("F5 - hot reload game DLL");
    ImGui::BulletText("ESC - release mouse");
    ImGui::Separator();

    auto view = ctx->registry->view<as3::CameraComponent>();
    for (auto entity : view)
    {
        auto& cam = view.get<as3::CameraComponent>(entity);
        ImGui::Text("Pos: %.1f, %.1f, %.1f", cam.position.x, cam.position.y, cam.position.z);
        ImGui::SliderFloat("FOV", &cam.fov, 30.0f, 110.0f);
        ImGui::SliderFloat("Speed", &cam.move_speed, 5.0f, 40.0f);
    }

    ImGui::End();
    
    // Music player window
    if (ctx->audio && !g_music_names.empty())
    {
        ImGui::Begin("Music Player");
        
        // Volume control
        float vol = ctx->audio->get_music_volume();
        if (ImGui::SliderFloat("Volume", &vol, 0.0f, 1.0f))
        {
            ctx->audio->set_music_volume(vol);
        }
        
        // Play/Pause/Stop buttons
        bool playing = ctx->audio->is_music_playing();
        bool paused = ctx->audio->is_music_paused();
        
        if (playing && !paused)
        {
            if (ImGui::Button("Pause"))
                ctx->audio->pause_music();
        }
        else if (paused)
        {
            if (ImGui::Button("Resume"))
                ctx->audio->resume_music();
        }
        else
        {
            if (ImGui::Button("Play") && g_current_track >= 0)
                ctx->audio->play_music(g_music_tracks[static_cast<size_t>(g_current_track)]);
        }
        
        ImGui::SameLine();
        if (ImGui::Button("Stop"))
            ctx->audio->stop_music();
        
        // Track list
        ImGui::Separator();
        ImGui::Text("Tracks:");
        
        for (size_t i = 0; i < g_music_names.size(); ++i)
        {
            bool is_current = (static_cast<int>(i) == g_current_track);
            
            if (is_current)
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 1.0f, 0.3f, 1.0f));
            
            if (ImGui::Selectable(g_music_names[i].c_str(), is_current))
            {
                g_current_track = static_cast<int>(i);
                if (g_music_tracks[i] != as3::invalid_music)
                {
                    ctx->audio->play_music(g_music_tracks[i]);
                }
            }
            
            if (is_current)
                ImGui::PopStyleColor();
        }
        
        ImGui::End();
    }
}
