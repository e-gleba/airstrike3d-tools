#include "camera.hpp"
#include "composite_model.hpp"
#include "editor.hpp"
#include "game_api.hpp"
#include "level.hpp"

#include <imgui.h>
#include <spdlog/spdlog.h>

#include <cmath>

namespace
{

as3::engine_context*          g_ctx = nullptr;
std::vector<as3::mesh_handle> g_meshes;

// Composite models (body + rotors)
as3::CompositeModel g_cobra_composite;
as3::CompositeModel g_mi24_composite;
as3::CompositeModel g_kamov_composite;

as3::model_handle g_tiger_tank = as3::invalid_model;

// Transforms for editor
as3::transform g_cobra_xform;
as3::transform g_mi24_xform;
as3::transform g_kamov_xform;
as3::transform g_tank_xform;

// Editor
as3::Editor g_editor;

// Level system
as3::LevelManager g_level_manager;
std::vector<std::string> g_level_list;
int g_selected_level = -1;
bool g_show_level_browser = false;

// Music
std::vector<as3::music_handle> g_music_tracks;
std::vector<std::string>       g_music_names;
int                            g_current_track = -1;

// State
entt::entity g_camera_entity = entt::null;
float        g_time          = 0.0f;
bool         g_wireframe     = false;

constexpr float MODEL_SCALE = 0.04f;

struct HeliConfig
{
    glm::vec3 base_pos;
    float     base_yaw;
    float     hover_freq;
    float     hover_amplitude;
};

constexpr HeliConfig g_cobra_cfg = { .base_pos        = { -15.0f, 4.0f, 0.0f },
                                     .base_yaw        = 15.0f,
                                     .hover_freq      = 0.8f,
                                     .hover_amplitude = 1.0f };

constexpr HeliConfig g_mi24_cfg = { .base_pos        = { 0.0f, 5.0f, 0.0f },
                                    .base_yaw        = -10.0f,
                                    .hover_freq      = 0.6f,
                                    .hover_amplitude = 0.8f };

constexpr HeliConfig g_kamov_cfg = { .base_pos        = { 15.0f, 4.5f, 0.0f },
                                     .base_yaw        = -20.0f,
                                     .hover_freq      = 0.7f,
                                     .hover_amplitude = 0.9f };

struct HoverState
{
    glm::vec3 position;
    float     pitch;
    float     yaw;
    float     roll;
};

[[nodiscard]] HoverState calculate_hover(const HeliConfig& cfg,
                                         float             time,
                                         float             phase)
{
    const float t   = time * cfg.hover_freq + phase;
    const float amp = cfg.hover_amplitude;

    HoverState state;
    state.position = cfg.base_pos + glm::vec3(std::sin(t) * 0.15f * amp,
                                              std::sin(t * 1.7f) * 0.12f * amp,
                                              std::cos(t * 0.8f) * 0.1f * amp);
    state.pitch    = std::sin(t * 0.9f) * 4.0f * amp;
    state.roll     = std::sin(t * 1.2f) * 3.5f * amp;
    state.yaw      = cfg.base_yaw + std::sin(t * 0.4f) * 2.0f * amp;

    return state;
}

void draw_helicopter(as3::CompositeModel& heli,
                     const HeliConfig&    cfg,
                     as3::transform&      xform,
                     float                time,
                     float                phase,
                     float                dt)
{
    if (!heli.is_loaded())
        return;

    heli.update(dt);
    const auto hover = calculate_hover(cfg, time, phase);
    
    // Update transform for editor
    xform.position = hover.position;
    xform.rotation = { hover.pitch, hover.yaw, hover.roll };
    xform.scale    = { MODEL_SCALE, MODEL_SCALE, MODEL_SCALE };
    
    heli.draw(g_ctx->renderer, xform);
}

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
    cam.position   = { 0.0f, 8.0f, 28.0f };
    cam.pitch      = -10.0f;
    cam.yaw        = -90.0f;
    cam.move_speed = 12.0f;

    g_meshes.push_back(g_ctx->renderer->create_wireframe_grid(
        100.0f, 100, { 0.12f, 0.16f, 0.12f }));
    g_meshes.push_back(g_ctx->renderer->create_wireframe_cube(
        { -15.0f, 0.02f, 0.0f }, 4.0f, { 0.2f, 0.2f, 0.5f }));
    g_meshes.push_back(g_ctx->renderer->create_wireframe_cube(
        { 0.0f, 0.02f, 0.0f }, 5.0f, { 0.2f, 0.2f, 0.5f }));
    g_meshes.push_back(g_ctx->renderer->create_wireframe_cube(
        { 15.0f, 0.02f, 0.0f }, 4.0f, { 0.2f, 0.2f, 0.5f }));

    // Load composite models from YAML configs (or fallback to defaults)
    // Cobra
    constexpr const char* cobra_config = "assets/configs/helicopters/cobra.yaml";
    if (!g_cobra_composite.load(g_ctx->renderer, cobra_config))
    {
        as3::composite_model_config cfg;
        cfg.name      = "Cobra";
        cfg.body_path = "assets/models/helics/cobra/cobra.obj";
        g_cobra_composite.load(g_ctx->renderer, cfg);
    }

    // MI-24
    constexpr const char* mi24_config = "assets/configs/helicopters/mi-24.yaml";
    if (!g_mi24_composite.load(g_ctx->renderer, mi24_config))
    {
        as3::composite_model_config cfg;
        cfg.name      = "MI-24 Hind";
        cfg.body_path = "assets/models/helics/mi_24/mi_24.obj";
        cfg.attachments.push_back({
            .name           = "main_rotor",
            .model_path     = "assets/models/helics/vints/vint_c.obj",
            .texture_path   = "",
            .offset         = { 0.0f, 2.0f, 1.0f },
            .rotation_axis  = { 0.0f, 1.0f, 0.0f },
            .rotation_speed = 600.0f,
            .scale          = 1.2f
        });
        g_mi24_composite.load(g_ctx->renderer, cfg);
    }

    // Kamov
    constexpr const char* kamov_config = "assets/configs/helicopters/kamov.yaml";
    if (!g_kamov_composite.load(g_ctx->renderer, kamov_config))
    {
        as3::composite_model_config cfg;
        cfg.name      = "Kamov Ka-50";
        cfg.body_path = "assets/models/helics/kamov/kamov.obj";
        cfg.attachments.push_back({
            .name           = "main_rotor",
            .model_path     = "assets/models/helics/vints/vint_a2.obj",
            .texture_path   = "assets/models/helics/vints/vint_3_red.tga",
            .offset         = { 0.0f, 1.8f, 0.0f },
            .rotation_axis  = { 0.0f, 1.0f, 0.0f },
            .rotation_speed = 540.0f,
            .scale          = 1.0f
        });
        g_kamov_composite.load(g_ctx->renderer, cfg);
    }

    // Tank
    g_tiger_tank = g_ctx->renderer->load_model(
        "assets/models/tanks/tiger/tiger_base.obj", { 0.5f, 0.45f, 0.25f });
    g_tank_xform.position = { 20.0f, 0.0f, 12.0f };
    g_tank_xform.rotation = { 0.0f, -120.0f, 0.0f };
    g_tank_xform.scale    = { MODEL_SCALE, MODEL_SCALE, MODEL_SCALE };

    // Initialize editor with config paths for saving
    g_editor.init(g_ctx);
    g_editor.register_composite("Cobra", &g_cobra_composite, &g_cobra_xform, cobra_config);
    g_editor.register_composite("MI-24", &g_mi24_composite, &g_mi24_xform, mi24_config);
    g_editor.register_composite("Kamov", &g_kamov_composite, &g_kamov_xform, kamov_config);
    g_editor.register_model("Tiger Tank", g_tiger_tank, &g_tank_xform);

    // Initialize level manager
    g_level_manager.init(g_ctx->renderer);
    g_level_list = g_level_manager.list_levels("assets/maps");

    g_ctx->renderer->set_render_mode(as3::render_mode::textured);

    if (g_ctx->audio)
    {
        g_music_names = g_ctx->audio->list_music_files();
        for (const auto& name : g_music_names)
            g_music_tracks.push_back(
                g_ctx->audio->load_music("assets/music/" + name));

        if (!g_music_tracks.empty() && g_music_tracks[0] != as3::invalid_music)
        {
            g_current_track = 0;
            g_ctx->audio->play_music(g_music_tracks[0]);
        }
    }

    spdlog::info("Game initialized");
    return true;
}

GAME_API void game_shutdown()
{
    g_level_manager.shutdown();
    g_editor.shutdown();

    for (auto h : g_meshes)
        if (h != as3::invalid_mesh)
            g_ctx->renderer->destroy_mesh(h);
    g_meshes.clear();

    // Unload composite models
    g_cobra_composite.unload(g_ctx->renderer);
    g_mi24_composite.unload(g_ctx->renderer);
    g_kamov_composite.unload(g_ctx->renderer);

    if (g_tiger_tank != as3::invalid_model)
    {
        g_ctx->renderer->unload_model(g_tiger_tank);
        g_tiger_tank = as3::invalid_model;
    }

    if (g_camera_entity != entt::null &&
        g_ctx->registry->valid(g_camera_entity))
    {
        g_ctx->registry->destroy(g_camera_entity);
        g_camera_entity = entt::null;
    }

    if (g_ctx->audio)
    {
        g_ctx->audio->stop_music();
        for (auto h : g_music_tracks)
            if (h != as3::invalid_music)
                g_ctx->audio->unload_music(h);
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
    ctx->renderer->set_render_mode(g_wireframe ? as3::render_mode::wireframe
                                               : as3::render_mode::textured);
}

GAME_API void game_render(as3::engine_context* ctx)
{
    // Draw level if loaded
    if (g_level_manager.has_level())
    {
        g_level_manager.render();
    }
    else
    {
        // Draw default scene (no level loaded)
        for (auto h : g_meshes)
            ctx->renderer->draw(h);

        draw_helicopter(
            g_cobra_composite, g_cobra_cfg, g_cobra_xform, g_time, 0.0f, ctx->delta_time);
        draw_helicopter(
            g_mi24_composite, g_mi24_cfg, g_mi24_xform, g_time, 2.1f, ctx->delta_time);
        draw_helicopter(
            g_kamov_composite, g_kamov_cfg, g_kamov_xform, g_time, 4.7f, ctx->delta_time);

        if (g_tiger_tank != as3::invalid_model)
            ctx->renderer->draw_model(g_tiger_tank, g_tank_xform);
    }

    // Draw editor gizmos
    g_editor.draw_gizmos();
}

GAME_API void game_ui(as3::engine_context* ctx)
{
    ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx->imgui_ctx));

    // Main info panel
    ImGui::SetNextWindowSize(ImVec2(280, 280), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::Begin("Airstrike 3D", nullptr, ImGuiWindowFlags_NoResize);

    const float fps = 1.0f / ctx->delta_time;
    ImGui::Text("FPS: %.0f (%.2f ms)", fps, ctx->delta_time * 1000.0f);

    // Render stats
    const auto stats = ctx->renderer->get_stats();
    ImGui::Separator();
    ImGui::TextColored({ 0.6f, 0.8f, 1.0f, 1.0f }, "Render Stats:");
    ImGui::Text("Draw calls: %u | Tris: %u", stats.draw_calls, stats.triangles);
    ImGui::Text("Models: %u | Textures: %u", stats.models_loaded, stats.textures_loaded);

    ImGui::Separator();
    ImGui::Checkbox("Wireframe Mode", &g_wireframe);
    
    bool editor_visible = g_editor.is_visible();
    if (ImGui::Checkbox("Scene Editor", &editor_visible))
        g_editor.set_visible(editor_visible);

    ImGui::Checkbox("Level Browser", &g_show_level_browser);

    // Level info
    if (g_level_manager.has_level())
    {
        ImGui::Separator();
        ImGui::TextColored({ 1.0f, 0.6f, 0.2f, 1.0f }, "Level: %s", 
                          g_level_manager.level().header().name.c_str());
        ImGui::Text("Objects: %zu", g_level_manager.level().objects().size());
    }

    // Camera info
    if (g_camera_entity != entt::null && g_ctx->registry->valid(g_camera_entity))
    {
        const auto& cam = g_ctx->registry->get<as3::CameraComponent>(g_camera_entity);
        ImGui::Separator();
        ImGui::TextColored({ 1.0f, 0.9f, 0.5f, 1.0f }, "Camera:");
        ImGui::Text("Pos: %.1f, %.1f, %.1f", cam.position.x, cam.position.y, cam.position.z);
    }

    ImGui::Separator();
    ImGui::TextColored({ 0.4f, 1.0f, 0.4f, 1.0f }, "Controls:");
    ImGui::BulletText("WASD/QE - fly camera");
    ImGui::BulletText("Mouse - look (click)");
    ImGui::BulletText("F5 - hot reload DLL");

    ImGui::End();

    // Scene Editor (separate window)
    g_editor.draw_ui();

    // Level Browser window
    if (g_show_level_browser)
    {
        ImGui::SetNextWindowSize(ImVec2(300, 400), ImGuiCond_FirstUseEver);
        ImGui::Begin("Level Browser", &g_show_level_browser);

        ImGui::TextColored({ 0.5f, 0.8f, 1.0f, 1.0f }, "Available Levels (%zu)", g_level_list.size());
        ImGui::Separator();

        ImGui::BeginChild("##levellist", ImVec2(0, -60), ImGuiChildFlags_Borders);
        for (size_t i = 0; i < g_level_list.size(); ++i)
        {
            bool selected = (static_cast<int>(i) == g_selected_level);
            if (ImGui::Selectable(g_level_list[i].c_str(), selected))
                g_selected_level = static_cast<int>(i);

            // Double-click to load
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
            {
                std::string path = "assets/maps/" + g_level_list[i] + ".yaml";
                g_level_manager.load_level(path);
            }
        }
        ImGui::EndChild();

        // Buttons
        if (g_selected_level >= 0 && g_selected_level < static_cast<int>(g_level_list.size()))
        {
            if (ImGui::Button("Load Level", ImVec2(-1, 0)))
            {
                std::string path = "assets/maps/" + g_level_list[static_cast<size_t>(g_selected_level)] + ".yaml";
                g_level_manager.load_level(path);
            }
        }
        else
        {
            ImGui::BeginDisabled();
            ImGui::Button("Load Level", ImVec2(-1, 0));
            ImGui::EndDisabled();
        }

        if (g_level_manager.has_level())
        {
            if (ImGui::Button("Unload Level", ImVec2(-1, 0)))
            {
                g_level_manager.unload_level();
            }
        }

        ImGui::End();
    }

    // Music player
    if (ctx->audio && !g_music_names.empty())
    {
        ImGui::Begin("Music Player");

        float vol = ctx->audio->get_music_volume();
        if (ImGui::SliderFloat("Volume", &vol, 0.0f, 1.0f))
            ctx->audio->set_music_volume(vol);

        bool playing = ctx->audio->is_music_playing();
        bool paused  = ctx->audio->is_music_paused();

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
                ctx->audio->play_music(
                    g_music_tracks[static_cast<size_t>(g_current_track)]);
        }

        ImGui::SameLine();
        if (ImGui::Button("Stop"))
            ctx->audio->stop_music();

        ImGui::Separator();
        ImGui::Text("Tracks:");

        for (size_t i = 0; i < g_music_names.size(); ++i)
        {
            bool is_current = (static_cast<int>(i) == g_current_track);
            if (is_current)
                ImGui::PushStyleColor(ImGuiCol_Text,
                                      ImVec4(0.3f, 1.0f, 0.3f, 1.0f));
            if (ImGui::Selectable(g_music_names[i].c_str(), is_current))
            {
                g_current_track = static_cast<int>(i);
                if (g_music_tracks[i] != as3::invalid_music)
                    ctx->audio->play_music(g_music_tracks[i]);
            }
            if (is_current)
                ImGui::PopStyleColor();
        }

        ImGui::End();
    }
}