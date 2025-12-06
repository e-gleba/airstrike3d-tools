#include <algorithm>
#include <core-api/camera.hpp>
#include <core-api/game.hpp>

#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>
#include <spdlog/spdlog.h>

#include <cmath>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace
{

// SDL scancodes (stable values from SDL3)
constexpr int key_w      = 26;
constexpr int key_a      = 4;
constexpr int key_s      = 22;
constexpr int key_d      = 7;
constexpr int key_q      = 20;
constexpr int key_e      = 8;
constexpr int key_escape = 41;
constexpr int key_f5     = 62;
constexpr int key_f11    = 68;
constexpr int key_lshift = 225;
constexpr int key_tab    = 43;

// ============================================================================
// State
// ============================================================================

euengine::engine_context*          g_ctx = nullptr;
std::vector<euengine::mesh_handle> g_meshes;
float                              g_time = 0.0f;

// Camera
entt::entity g_camera_entity = entt::null;
bool         g_camera_free   = true;

// Models
struct loaded_model
{
    euengine::model_handle handle = euengine::invalid_model;
    std::string            name;
    std::string            path;
    euengine::transform    transform;
    euengine::bounds       bounds;
};
std::vector<loaded_model> g_models;
int                       g_selected_model = -1;

// Music
struct music_track
{
    euengine::music_handle handle = euengine::invalid_music;
    std::string            name;
    std::string            path;
};
std::vector<music_track> g_music_tracks;
int                      g_current_track = -1;

// UI state
bool g_show_settings      = true;
bool g_show_model_browser = true;
bool g_show_music_browser = true;
bool g_show_inspector     = true;
bool g_wireframe          = false;

// Model browser
std::string              g_model_dir = "assets/models";
std::vector<std::string> g_model_files;
int                      g_browser_selected = -1;

// ============================================================================
// Model browser helpers
// ============================================================================

void scan_model_directory(const std::string& dir)
{
    g_model_files.clear();
    g_browser_selected = -1;

    if (!std::filesystem::exists(dir))
    {
        return;
    }

    for (const auto& entry : std::filesystem::recursive_directory_iterator(dir))
    {
        if (!entry.is_regular_file())
        {
            continue;
        }

        auto ext = entry.path().extension().string();
        if (ext == ".obj" || ext == ".OBJ" || ext == ".glb" || ext == ".GLB" ||
            ext == ".gltf" || ext == ".GLTF")
        {
            g_model_files.push_back(entry.path().string());
        }
    }

    std::sort(g_model_files.begin(), g_model_files.end());
    spdlog::info("Found {} model files in {}", g_model_files.size(), dir);
}

void scan_music_directory()
{
    const std::string dir = "assets/music";
    if (!std::filesystem::exists(dir))
    {
        return;
    }

    for (const auto& entry : std::filesystem::directory_iterator(dir))
    {
        if (!entry.is_regular_file())
        {
            continue;
        }

        auto ext = entry.path().extension().string();
        if (ext == ".ogg" || ext == ".OGG" || ext == ".mp3" || ext == ".MP3" ||
            ext == ".wav" || ext == ".WAV")
        {
            music_track track;
            track.name = entry.path().filename().string();
            track.path = entry.path().string();
            g_music_tracks.push_back(track);
        }
    }

    std::ranges::sort(g_music_tracks,
                      [](const auto& a, const auto& b)
                      { return a.name < b.name; });
    spdlog::info("Found {} music tracks", g_music_tracks.size());
}

void load_model_at(const std::string& path, const glm::vec3& pos)
{
    auto handle = g_ctx->renderer->load_model(path);
    if (handle == euengine::invalid_model)
    {
        spdlog::error("Failed to load model: {}", path);
        return;
    }

    loaded_model model;
    model.handle             = handle;
    model.path               = path;
    model.name               = std::filesystem::path(path).stem().string();
    model.bounds             = g_ctx->renderer->get_bounds(handle);
    model.transform.position = pos;
    model.transform.scale    = glm::vec3(1.0f);

    g_models.push_back(model);
    g_selected_model = static_cast<int>(g_models.size()) - 1;

    spdlog::info("Loaded model: {} at ({:.1f}, {:.1f}, {:.1f})",
                 model.name,
                 pos.x,
                 pos.y,
                 pos.z);
}

// ============================================================================
// Camera control
// ============================================================================

void update_camera(euengine::engine_context* ctx)
{
    if (g_camera_entity == entt::null || !ctx->registry->valid(g_camera_entity))
    {
        return;
    }

    auto& cam = ctx->registry->get<euengine::camera_component>(g_camera_entity);

    // Mouse look (when captured)
    if (ctx->input.mouse_captured)
    {
        cam.yaw += ctx->input.mouse_xrel * cam.look_speed;
        cam.pitch -= ctx->input.mouse_yrel * cam.look_speed;
        cam.pitch = glm::clamp(cam.pitch, -89.0f, 89.0f);
    }

    // Keyboard movement
    if ((ctx->input.keyboard != nullptr) && g_camera_free)
    {
        float     speed = cam.move_speed * ctx->delta_time;
        glm::vec3 front = cam.front();
        glm::vec3 right = cam.right();
        glm::vec3 up(0.0f, 1.0f, 0.0f);

        // Shift for faster movement
        if (ctx->input.keyboard[key_lshift])
        {
            speed *= 3.0f;
        }

        if (ctx->input.keyboard[key_w])
        {
            cam.position += front * speed;
        }
        if (ctx->input.keyboard[key_s])
        {
            cam.position -= front * speed;
        }
        if (ctx->input.keyboard[key_a])
        {
            cam.position -= right * speed;
        }
        if (ctx->input.keyboard[key_d])
        {
            cam.position += right * speed;
        }
        if (ctx->input.keyboard[key_q])
        {
            cam.position -= up * speed;
        }
        if (ctx->input.keyboard[key_e])
        {
            cam.position += up * speed;
        }

        // Toggle mouse capture
        if (ctx->input.keyboard[key_escape])
        {
            ctx->settings->set_mouse_captured(false);
        }

        // Fullscreen toggle (simple debounce)
        static bool f11_was_pressed = false;
        if (ctx->input.keyboard[key_f11])
        {
            if (!f11_was_pressed)
            {
                ctx->settings->set_fullscreen(!ctx->settings->is_fullscreen());
                f11_was_pressed = true;
            }
        }
        else
        {
            f11_was_pressed = false;
        }

        // Hot reload (F5)
        static bool f5_was_pressed = false;
        if (ctx->input.keyboard[key_f5])
        {
            if (!f5_was_pressed)
            {
                spdlog::info("Hot reload triggered (F5)");

                // Rescan model directory
                scan_model_directory(g_model_dir);

                // If shader hot reload is enabled, shaders auto-reload
                // Just log that we triggered it
                if ((ctx->shaders != nullptr) &&
                    ctx->shaders->hot_reload_enabled())
                {
                    spdlog::info("Shader hot reload is enabled - shaders will "
                                 "reload if changed");
                }

                f5_was_pressed = true;
            }
        }
        else
        {
            f5_was_pressed = false;
        }

        // Wireframe toggle (Tab)
        static bool tab_was_pressed = false;
        if (ctx->input.keyboard[key_tab])
        {
            if (!tab_was_pressed)
            {
                g_wireframe     = !g_wireframe;
                tab_was_pressed = true;
            }
        }
        else
        {
            tab_was_pressed = false;
        }
    }

    // Update view-projection
    glm::mat4 view = cam.view();
    glm::mat4 proj = cam.projection(ctx->display.aspect);
    ctx->renderer->set_view_projection(proj * view);
}

} // namespace

// ============================================================================
// Game API implementation
// ============================================================================

/// Pre-initialization callback - configure engine settings before SDL init
/// This is called BEFORE the engine is initialized
GAME_API euengine::preinit_result game_preinit(
    euengine::preinit_settings* settings)
{
    spdlog::info("=> game_preinit");

    // Configure window settings
    settings->window.title     = "airstrike3d preview";
    settings->window.width     = 1600;
    settings->window.height    = 900;
    settings->window.mode      = euengine::window_mode::windowed;
    settings->window.vsync     = euengine::vsync_mode::enabled;
    settings->window.resizable = true;
    settings->window.high_dpi  = true;

    // Configure renderer settings
    settings->renderer.wireframe_mode  = false;
    settings->renderer.show_debug_info = false;

    // Configure audio settings
    settings->audio.master_volume = 0.8f;
    settings->audio.music_volume  = 0.5f;
    settings->audio.sound_volume  = 1.0f;

    return euengine::preinit_result::ok;
}

GAME_API bool game_init(euengine::engine_context* ctx)
{
    g_ctx  = ctx;
    g_time = 0.0f;

    // Setup camera
    if (g_camera_entity != entt::null && ctx->registry->valid(g_camera_entity))
    {
        ctx->registry->destroy(g_camera_entity);
    }

    g_camera_entity = ctx->registry->create();
    auto& cam =
        ctx->registry->emplace<euengine::camera_component>(g_camera_entity);
    cam.position   = { 0.0f, 15.0f, 25.0f };
    cam.pitch      = -25.0f;
    cam.yaw        = -90.0f;
    cam.move_speed = 15.0f;
    cam.look_speed = 0.15f;

    // Create ground grid
    g_meshes.push_back(ctx->renderer->create_wireframe_grid(
        100.0f, 50, { 0.2f, 0.25f, 0.2f }));

    // Scan directories
    scan_model_directory(g_model_dir);
    scan_music_directory();

    // Load some default models for demo
    load_model_at("assets/models/tanks/t72/t72_base.obj",
                  { -8.0f, 0.0f, 0.0f });
    load_model_at("assets/models/helics/kamov/kamov.obj", { 0.0f, 3.0f, 0.0f });
    load_model_at("assets/models/samples/duck.glb", { 8.0f, 0.0f, 0.0f });

    ctx->renderer->set_render_mode(euengine::render_mode::textured);

    spdlog::info("Demo initialized - WASD to move, mouse to look (click to "
                 "capture), F11 fullscreen");
    return true;
}

GAME_API void game_shutdown()
{
    // Unload models
    for (auto& model : g_models)
    {
        if (model.handle != euengine::invalid_model &&
            (g_ctx->renderer != nullptr))
        {
            g_ctx->renderer->unload_model(model.handle);
        }
    }
    g_models.clear();

    // Unload music
    for (auto& track : g_music_tracks)
    {
        if (track.handle != euengine::invalid_music &&
            (g_ctx->audio != nullptr))
        {
            g_ctx->audio->unload_music(track.handle);
        }
    }
    g_music_tracks.clear();

    // Destroy meshes
    for (auto h : g_meshes)
    {
        if (h != euengine::invalid_mesh && (g_ctx->renderer != nullptr))
        {
            g_ctx->renderer->destroy_mesh(h);
        }
    }
    g_meshes.clear();

    // Destroy camera
    if (g_camera_entity != entt::null &&
        g_ctx->registry->valid(g_camera_entity))
    {
        g_ctx->registry->destroy(g_camera_entity);
        g_camera_entity = entt::null;
    }

    spdlog::info("Demo shutdown");
    g_ctx = nullptr;
}

GAME_API void game_update(euengine::engine_context* ctx)
{
    g_time += ctx->delta_time;

    // Update render mode
    ctx->renderer->set_render_mode(g_wireframe
                                       ? euengine::render_mode::wireframe
                                       : euengine::render_mode::textured);

    // Camera
    update_camera(ctx);

    // Capture mouse on click (when not over ImGui)
    if (ctx->imgui_ctx != nullptr)
    {
        ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx->imgui_ctx));
        ImGuiIO& io = ImGui::GetIO();

        if (!io.WantCaptureMouse && io.MouseClicked[0])
        {
            ctx->settings->set_mouse_captured(true);
        }
    }

    // Animate some models for fun
    for (auto& model : g_models)
    {
        // Gentle hover animation for helicopters
        if (model.name.find("kamov") != std::string::npos ||
            model.name.find("helic") != std::string::npos ||
            model.name.find("mi_24") != std::string::npos)
        {
            model.transform.position.y = 3.0f + std::sin(g_time * 1.5f) * 0.3f;
        }
    }
}

GAME_API void game_render(euengine::engine_context* ctx)
{
    // Draw grid
    for (auto h : g_meshes)
    {
        ctx->renderer->draw(h);
    }

    // Draw models
    for (auto& model : g_models)
    {
        ctx->renderer->draw_model(model.handle, model.transform);

        // Draw bounds for selected model
        if (&model - g_models.data() == g_selected_model)
        {
            ctx->renderer->draw_bounds(
                model.bounds, model.transform, { 0.0f, 1.0f, 0.0f });
        }
    }
}

GAME_API void game_ui(euengine::engine_context* ctx)
{
    ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx->imgui_ctx));
    ImGuiIO& io = ImGui::GetIO();

    // ===== MAIN MENU BAR =====
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Quit", "Alt+F4"))
            {
                ctx->settings->request_quit();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View"))
        {
            ImGui::MenuItem("Inspector", nullptr, &g_show_inspector);
            ImGui::MenuItem("Model Browser", nullptr, &g_show_model_browser);
            ImGui::MenuItem("Music Player", nullptr, &g_show_music_browser);
            ImGui::MenuItem("Engine Settings", nullptr, &g_show_settings);
            ImGui::Separator();
            ImGui::MenuItem("Wireframe", "Tab", &g_wireframe);
            ImGui::EndMenu();
        }

        // FPS on right side
        float fps = 1.0f / ctx->delta_time;
        char  fps_text[32];
        snprintf(fps_text, sizeof(fps_text), "%.0f FPS", fps);
        float text_width = ImGui::CalcTextSize(fps_text).x;
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - text_width - 10);
        ImGui::TextColored(ImVec4(0.5f, 0.8f, 0.5f, 1.0f), "%s", fps_text);

        ImGui::EndMainMenuBar();
    }

    // ===== INSPECTOR =====
    if (g_show_inspector)
    {
        ImGui::SetNextWindowPos(ImVec2(10, 30), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(280, 400), ImGuiCond_FirstUseEver);

        if (ImGui::Begin("Inspector", &g_show_inspector))
        {
            // Scene stats
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Scene");
            ImGui::Separator();

            const auto stats = ctx->renderer->get_stats();
            ImGui::Text("Models: %zu", g_models.size());
            ImGui::Text("Draw calls: %u", stats.draw_calls);
            ImGui::Text("Triangles: %u", stats.triangles);

            ImGui::Spacing();

            // Camera info
            if (g_camera_entity != entt::null &&
                ctx->registry->valid(g_camera_entity))
            {
                auto& cam = ctx->registry->get<euengine::camera_component>(
                    g_camera_entity);
                ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Camera");
                ImGui::Separator();
                ImGui::Text("Pos: %.1f, %.1f, %.1f",
                            cam.position.x,
                            cam.position.y,
                            cam.position.z);
                ImGui::DragFloat("Speed", &cam.move_speed, 0.5f, 1.0f, 100.0f);
                ImGui::DragFloat("FOV", &cam.fov, 1.0f, 30.0f, 120.0f);
            }

            ImGui::Spacing();

            // Model list
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Loaded Models");
            ImGui::Separator();

            for (std::size_t i = 0; i < g_models.size(); ++i)
            {
                auto&      model    = g_models[i];
                const bool selected = (std::cmp_equal(i, g_selected_model));

                if (ImGui::Selectable(model.name.c_str(), selected))
                {
                    g_selected_model = static_cast<int>(i);
                }
            }

            // Selected model properties
            if (g_selected_model >= 0 &&
                static_cast<std::size_t>(g_selected_model) < g_models.size())
            {
                auto& model =
                    g_models[static_cast<std::size_t>(g_selected_model)];

                ImGui::Spacing();
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f),
                                   "Selected: %s",
                                   model.name.c_str());
                ImGui::Separator();

                ImGui::DragFloat3(
                    "Position", &model.transform.position.x, 0.1f);
                ImGui::DragFloat3(
                    "Rotation", &model.transform.rotation.x, 1.0f);

                float scale = model.transform.scale.x;
                if (ImGui::DragFloat("Scale", &scale, 0.01f, 0.01f, 10.0f))
                {
                    model.transform.scale = glm::vec3(scale);
                }

                ImGui::Spacing();
                if (ImGui::Button("Remove"))
                {
                    ctx->renderer->unload_model(model.handle);
                    g_models.erase(g_models.begin() + g_selected_model);
                    g_selected_model = -1;
                }
            }
        }
        ImGui::End();
    }

    // ===== MODEL BROWSER =====
    if (g_show_model_browser)
    {
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 320, 30),
                                ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(310, 350), ImGuiCond_FirstUseEver);

        if (ImGui::Begin("Model Browser", &g_show_model_browser))
        {
            // Directory input
            static char dir_buf[256];
            strncpy(dir_buf, g_model_dir.c_str(), sizeof(dir_buf) - 1);
            if (ImGui::InputText("Dir", dir_buf, sizeof(dir_buf)))
            {
                g_model_dir = dir_buf;
            }
            ImGui::SameLine();
            if (ImGui::Button("Scan"))
            {
                scan_model_directory(g_model_dir);
            }

            ImGui::Text("Found %zu models", g_model_files.size());
            ImGui::Separator();

            // File list
            ImGui::BeginChild(
                "ModelList", ImVec2(0, -30), 1, ImGuiWindowFlags_None);
            for (std::size_t i = 0; i < g_model_files.size(); ++i)
            {
                std::string display =
                    std::filesystem::path(g_model_files[i]).filename().string();
                if (ImGui::Selectable(display.c_str(),
                                      std::cmp_equal(i, g_browser_selected)))
                {
                    g_browser_selected = static_cast<int>(i);
                }
            }
            ImGui::EndChild();

            // Load button
            if (ImGui::Button("Load Selected") && g_browser_selected >= 0 &&
                static_cast<std::size_t>(g_browser_selected) <
                    g_model_files.size())
            {
                load_model_at(
                    g_model_files[static_cast<std::size_t>(g_browser_selected)],
                    { 0.0f, 0.0f, 0.0f });
            }
        }
        ImGui::End();
    }

    // ===== MUSIC PLAYER =====
    if (g_show_music_browser && (ctx->audio != nullptr))
    {
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 320, 400),
                                ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(310, 200), ImGuiCond_FirstUseEver);

        if (ImGui::Begin("Music Player", &g_show_music_browser))
        {
            // Track list
            ImGui::BeginChild(
                "TrackList", ImVec2(0, -60), 1, ImGuiWindowFlags_None);
            for (std::size_t i = 0; i < g_music_tracks.size(); ++i)
            {
                auto& track    = g_music_tracks[i];
                bool  playing  = (std::cmp_equal(i, g_current_track));
                bool  selected = playing;

                if (playing)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text,
                                          ImVec4(0.3f, 1.0f, 0.3f, 1.0f));
                }

                if (ImGui::Selectable(track.name.c_str(), selected))
                {
                    // Load if needed
                    if (track.handle == euengine::invalid_music)
                    {
                        track.handle = ctx->audio->load_music(track.path);
                    }

                    if (track.handle != euengine::invalid_music)
                    {
                        ctx->audio->play_music(track.handle, true);
                        g_current_track = static_cast<int>(i);
                    }
                }

                if (playing)
                {
                    ImGui::PopStyleColor();
                }
            }
            ImGui::EndChild();

            // Playback controls
            bool is_playing = ctx->audio->is_music_playing();
            bool is_paused  = ctx->audio->is_music_paused();

            if (is_playing && !is_paused)
            {
                if (ImGui::Button("Pause"))
                {
                    ctx->audio->pause_music();
                }
            }
            else if (is_paused)
            {
                if (ImGui::Button("Resume"))
                {
                    ctx->audio->resume_music();
                }
            }

            ImGui::SameLine();
            if (ImGui::Button("Stop"))
            {
                ctx->audio->stop_music();
                g_current_track = -1;
            }

            // Volume
            float vol = ctx->audio->get_music_volume();
            if (ImGui::SliderFloat("Volume", &vol, 0.0f, 1.0f))
            {
                ctx->audio->set_music_volume(vol);
            }
        }
        ImGui::End();
    }

    // ===== ENGINE SETTINGS =====
    if (g_show_settings && (ctx->settings != nullptr))
    {
        ImGui::SetNextWindowPos(ImVec2(10, 450), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(280, 220), ImGuiCond_FirstUseEver);

        if (ImGui::Begin("Engine Settings", &g_show_settings))
        {
            ImGui::Text("Resolution: %dx%d",
                        ctx->settings->get_window_width(),
                        ctx->settings->get_window_height());
            ImGui::Text("GPU: %s", ctx->settings->get_gpu_driver().data());

            ImGui::Spacing();

            bool fullscreen = ctx->settings->is_fullscreen();
            if (ImGui::Checkbox("Fullscreen (F11)", &fullscreen))
            {
                ctx->settings->set_fullscreen(fullscreen);
            }

            ImGui::Spacing();
            ImGui::Text("VSync:");
            int vsync = static_cast<int>(ctx->settings->get_vsync());
            if (ImGui::RadioButton("Off", &vsync, 0))
            {
                ctx->settings->set_vsync(euengine::vsync_mode::disabled);
            }
            ImGui::SameLine();
            if (ImGui::RadioButton("On", &vsync, 1))
            {
                ctx->settings->set_vsync(euengine::vsync_mode::enabled);
            }
            ImGui::SameLine();
            if (ImGui::RadioButton("Adaptive", &vsync, 2))
            {
                ctx->settings->set_vsync(euengine::vsync_mode::adaptive);
            }

            // Shader hot reload
            if (ctx->shaders != nullptr)
            {
                ImGui::Spacing();
                ImGui::Separator();
                bool hot = ctx->shaders->hot_reload_enabled();
                if (ImGui::Checkbox("Shader Hot Reload", &hot))
                {
                    ctx->shaders->enable_hot_reload(hot);
                }
            }

            // Audio
            if (ctx->audio != nullptr)
            {
                ImGui::Spacing();
                ImGui::Separator();
                float sound_vol = ctx->audio->get_sound_volume();
                if (ImGui::SliderFloat("Sound Vol", &sound_vol, 0.0f, 1.0f))
                {
                    ctx->audio->set_sound_volume(sound_vol);
                }
            }
        }
        ImGui::End();
    }

    // ===== STATUS BAR =====
    ImGui::SetNextWindowPos(ImVec2(0, io.DisplaySize.y - 25));
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, 25));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 4));
    if (ImGui::Begin("##statusbar",
                     nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                         ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoScrollbar))
    {
        const char* help =
            ctx->input.mouse_captured
                ? "WASD - move | QE - up/down | Shift - fast | Tab - wireframe "
                  "| F5 - reload | F11 - fullscreen | ESC - release mouse"
                : "Click to capture mouse | WASD - move | Tab - wireframe | "
                  "F5 - reload | F11 - fullscreen";

        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%s", help);
    }
    ImGui::End();
    ImGui::PopStyleVar();
}
