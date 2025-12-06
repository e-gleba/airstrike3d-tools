#include <algorithm>
#include <core-api/camera.hpp>
#include <core-api/game.hpp>

#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>
#include <spdlog/spdlog.h>

#include <cmath>
#include <filesystem>
#include <numbers>
#include <string>
#include <utility>
#include <vector>

namespace
{

// SDL scancodes
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
constexpr int key_space  = 44;
constexpr int key_1      = 30;
constexpr int key_2      = 31;
constexpr int key_3      = 32;

// ============================================================================
// Data structures
// ============================================================================

struct showcase_model
{
    euengine::model_handle handle = euengine::invalid_model;
    std::string            name;
    std::string            path;
    euengine::transform    transform;
    euengine::bounds       bounds;
    bool                   rotate       = false;
    float                  rotate_speed = 30.0f;
    bool                   hover        = false;
    float                  hover_height = 0.0f;
    float                  hover_speed  = 1.5f;
    float                  hover_amount = 0.3f;
};

struct music_track
{
    euengine::music_handle handle = euengine::invalid_music;
    std::string            name;
    std::string            path;
};

// ============================================================================
// Global state
// ============================================================================

euengine::engine_context*          g_ctx = nullptr;
std::vector<euengine::mesh_handle> g_meshes;
entt::entity                       g_camera_entity = entt::null;

std::vector<showcase_model> g_showcase;
int                         g_selected_model = -1;

std::vector<music_track> g_music_tracks;
int                      g_current_track = -1;

// UI panels
bool g_show_hierarchy  = true;
bool g_show_properties = true;
bool g_show_browser    = false;
bool g_show_music      = true;

// Scene settings
bool  g_wireframe   = false;
bool  g_auto_rotate = true;
int   g_sky_preset  = 1; // Start with day sky
float g_sky_color[3] = { 0.4f, 0.6f, 0.9f };

// Browser
std::string              g_model_dir = "assets/models";
std::vector<std::string> g_model_files;
int                      g_browser_selected = -1;

// ============================================================================
// ImGui Theme
// ============================================================================

void setup_imgui_style()
{
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4*     colors = style.Colors;

    // Modern dark theme with subtle accents
    colors[ImGuiCol_Text]                  = ImVec4(0.92f, 0.92f, 0.92f, 1.00f);
    colors[ImGuiCol_TextDisabled]          = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_WindowBg]              = ImVec4(0.13f, 0.13f, 0.15f, 0.95f);
    colors[ImGuiCol_ChildBg]               = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
    colors[ImGuiCol_PopupBg]               = ImVec4(0.13f, 0.13f, 0.15f, 0.98f);
    colors[ImGuiCol_Border]                = ImVec4(0.25f, 0.25f, 0.28f, 0.50f);
    colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]               = ImVec4(0.20f, 0.20f, 0.22f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.28f, 0.28f, 0.30f, 1.00f);
    colors[ImGuiCol_FrameBgActive]         = ImVec4(0.35f, 0.35f, 0.38f, 1.00f);
    colors[ImGuiCol_TitleBg]               = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
    colors[ImGuiCol_TitleBgActive]         = ImVec4(0.15f, 0.15f, 0.17f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.10f, 0.10f, 0.12f, 0.75f);
    colors[ImGuiCol_MenuBarBg]             = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.30f, 0.30f, 0.32f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.40f, 0.40f, 0.42f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.50f, 0.50f, 0.52f, 1.00f);
    colors[ImGuiCol_CheckMark]             = ImVec4(0.40f, 0.70f, 0.45f, 1.00f);
    colors[ImGuiCol_SliderGrab]            = ImVec4(0.40f, 0.55f, 0.70f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.50f, 0.65f, 0.80f, 1.00f);
    colors[ImGuiCol_Button]                = ImVec4(0.25f, 0.25f, 0.28f, 1.00f);
    colors[ImGuiCol_ButtonHovered]         = ImVec4(0.35f, 0.35f, 0.38f, 1.00f);
    colors[ImGuiCol_ButtonActive]          = ImVec4(0.40f, 0.55f, 0.70f, 1.00f);
    colors[ImGuiCol_Header]                = ImVec4(0.25f, 0.25f, 0.28f, 1.00f);
    colors[ImGuiCol_HeaderHovered]         = ImVec4(0.35f, 0.45f, 0.55f, 1.00f);
    colors[ImGuiCol_HeaderActive]          = ImVec4(0.40f, 0.55f, 0.70f, 1.00f);
    colors[ImGuiCol_Separator]             = ImVec4(0.25f, 0.25f, 0.28f, 1.00f);
    colors[ImGuiCol_SeparatorHovered]      = ImVec4(0.40f, 0.55f, 0.70f, 1.00f);
    colors[ImGuiCol_SeparatorActive]       = ImVec4(0.50f, 0.65f, 0.80f, 1.00f);
    colors[ImGuiCol_ResizeGrip]            = ImVec4(0.25f, 0.25f, 0.28f, 0.50f);
    colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.40f, 0.55f, 0.70f, 0.75f);
    colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.50f, 0.65f, 0.80f, 1.00f);
    colors[ImGuiCol_Tab]                   = ImVec4(0.18f, 0.18f, 0.20f, 1.00f);
    colors[ImGuiCol_TabHovered]            = ImVec4(0.35f, 0.45f, 0.55f, 1.00f);
    colors[ImGuiCol_TabActive]             = ImVec4(0.25f, 0.35f, 0.45f, 1.00f);
    colors[ImGuiCol_TabUnfocused]          = ImVec4(0.15f, 0.15f, 0.17f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive]    = ImVec4(0.20f, 0.20f, 0.22f, 1.00f);
    colors[ImGuiCol_PlotLines]             = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered]      = ImVec4(0.40f, 0.70f, 0.45f, 1.00f);
    colors[ImGuiCol_PlotHistogram]         = ImVec4(0.40f, 0.55f, 0.70f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(0.50f, 0.65f, 0.80f, 1.00f);
    colors[ImGuiCol_TableHeaderBg]         = ImVec4(0.18f, 0.18f, 0.20f, 1.00f);
    colors[ImGuiCol_TableBorderStrong]     = ImVec4(0.25f, 0.25f, 0.28f, 1.00f);
    colors[ImGuiCol_TableBorderLight]      = ImVec4(0.20f, 0.20f, 0.22f, 1.00f);
    colors[ImGuiCol_TableRowBg]            = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt]         = ImVec4(1.00f, 1.00f, 1.00f, 0.03f);
    colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.40f, 0.55f, 0.70f, 0.35f);
    colors[ImGuiCol_DragDropTarget]        = ImVec4(0.40f, 0.70f, 0.45f, 0.90f);
    colors[ImGuiCol_NavHighlight]          = ImVec4(0.40f, 0.55f, 0.70f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.20f, 0.20f, 0.20f, 0.35f);

    // Style adjustments
    style.WindowRounding    = 6.0f;
    style.ChildRounding     = 4.0f;
    style.FrameRounding     = 4.0f;
    style.PopupRounding     = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.GrabRounding      = 4.0f;
    style.TabRounding       = 4.0f;

    style.WindowPadding     = ImVec2(10, 10);
    style.FramePadding      = ImVec2(8, 4);
    style.ItemSpacing       = ImVec2(8, 6);
    style.ItemInnerSpacing  = ImVec2(6, 4);
    style.IndentSpacing     = 20.0f;
    style.ScrollbarSize     = 14.0f;
    style.GrabMinSize       = 12.0f;

    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize  = 1.0f;
    style.PopupBorderSize  = 1.0f;
    style.FrameBorderSize  = 0.0f;
    style.TabBorderSize    = 0.0f;
}

void apply_sky_preset(int preset)
{
    if (g_ctx == nullptr || g_ctx->background == nullptr)
    {
        return;
    }

    switch (preset)
    {
        case 0: // Dark
            *g_ctx->background = { 0.08f, 0.08f, 0.12f, 1.0f };
            g_sky_color[0] = 0.08f; g_sky_color[1] = 0.08f; g_sky_color[2] = 0.12f;
            break;
        case 1: // Day
            *g_ctx->background = { 0.4f, 0.6f, 0.9f, 1.0f };
            g_sky_color[0] = 0.4f; g_sky_color[1] = 0.6f; g_sky_color[2] = 0.9f;
            break;
        case 2: // Sunset
            *g_ctx->background = { 0.95f, 0.5f, 0.3f, 1.0f };
            g_sky_color[0] = 0.95f; g_sky_color[1] = 0.5f; g_sky_color[2] = 0.3f;
            break;
        case 3: // Night
            *g_ctx->background = { 0.02f, 0.02f, 0.05f, 1.0f };
            g_sky_color[0] = 0.02f; g_sky_color[1] = 0.02f; g_sky_color[2] = 0.05f;
            break;
    }
}

// ============================================================================
// Helpers
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
    spdlog::info("found {} models", g_model_files.size());
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
        if (ext == ".ogg" || ext == ".mp3" || ext == ".wav" ||
            ext == ".OGG" || ext == ".MP3" || ext == ".WAV")
        {
            music_track track;
            track.name = entry.path().filename().string();
            track.path = entry.path().string();
            g_music_tracks.push_back(track);
        }
    }

    std::ranges::sort(g_music_tracks,
                      [](const auto& a, const auto& b) { return a.name < b.name; });
    spdlog::info("found {} tracks", g_music_tracks.size());
}

showcase_model* load_showcase_model(const std::string& path,
                                    const glm::vec3&   pos,
                                    bool               rotate = false,
                                    bool               hover  = false)
{
    auto handle = g_ctx->renderer->load_model(path);
    if (handle == euengine::invalid_model)
    {
        spdlog::error("load failed: {}", path);
        return nullptr;
    }

    showcase_model model;
    model.handle             = handle;
    model.path               = path;
    model.name               = std::filesystem::path(path).stem().string();
    model.bounds             = g_ctx->renderer->get_bounds(handle);
    model.transform.position = pos;
    model.transform.scale    = glm::vec3(1.0f);
    model.rotate             = rotate;
    model.hover              = hover;
    model.hover_height       = pos.y;

    g_showcase.push_back(std::move(model));
    return &g_showcase.back();
}

void setup_showcase_scene()
{
    g_meshes.push_back(
        g_ctx->renderer->create_wireframe_grid(120.0f, 60, { 0.15f, 0.2f, 0.15f }));

    constexpr float radius = 12.0f;
    constexpr int   n      = 8;

    auto pos = [](int i, float r, float h = 0.0f) {
        float a = float(i) * (2.0f * std::numbers::pi_v<float> / n);
        return glm::vec3 { r * std::cos(a), h, r * std::sin(a) };
    };

    // Center helicopter
    if (auto* m = load_showcase_model(
            "assets/models/helics/kamov/kamov.obj", { 0, 4, 0 }, true, true))
    {
        m->rotate_speed = 20.0f;
        m->hover_amount = 0.5f;
    }

    // Circle of vehicles
    load_showcase_model("assets/models/tanks/t72/t72_base.obj", pos(0, radius), true);
    if (auto* m = load_showcase_model(
            "assets/models/helics/mi_24/mi_24.obj", pos(1, radius, 3), true, true))
    {
        m->hover_speed = 1.2f;
    }
    load_showcase_model("assets/models/jeeps/uaz/uaz.obj", pos(2, radius), true);
    load_showcase_model(
        "assets/models/cannons/aagunvulcan/aagunvulcan_base.obj", pos(3, radius), true);
    load_showcase_model(
        "assets/models/tanks/sherman/sherman_base.obj", pos(4, radius), true);
    if (auto* m = load_showcase_model(
            "assets/models/helics/cobra/cobra.obj", pos(5, radius, 3.5f), true, true))
    {
        m->hover_speed = 1.8f;
    }
    load_showcase_model(
        "assets/models/btrs/btr_rocket/btr_rocket.obj", pos(6, radius), true);
    load_showcase_model(
        "assets/models/rocket_launcher_big/rocket_launcher_big.obj", pos(7, radius), true);

    // Outer buildings
    load_showcase_model("assets/models/mapobjects/cisterns/cisterna01.obj", { 25, 0, 0 });
    load_showcase_model("assets/models/mapobjects/houses/temple.obj", { -25, 0, 0 });
    if (auto* m = load_showcase_model(
            "assets/models/mapobjects/radar/radar.obj", { 0, 0, 25 }, true))
    {
        m->rotate_speed = 45.0f;
    }
    load_showcase_model(
        "assets/models/mapobjects/factory/oil_refinery/oil_refinery.obj", { 0, 0, -25 });

    // Corner decorations
    load_showcase_model("assets/models/mapobjects/barrel/barrel.obj", { 18, 0, 18 });
    load_showcase_model("assets/models/mapobjects/sandbags/sand_bags.obj", { -18, 0, 18 });
    load_showcase_model(
        "assets/models/mapobjects/stones/stone3_gray_big.obj", { 18, 0, -18 });
    load_showcase_model("assets/models/mapobjects/cactus/cactus_big.obj", { -18, 0, -18 });

    // glTF sample
    load_showcase_model("assets/models/samples/duck.glb", { -8, 1, 8 }, true);
}

void update_camera(euengine::engine_context* ctx)
{
    if (g_camera_entity == entt::null || !ctx->registry->valid(g_camera_entity))
    {
        return;
    }

    auto& cam = ctx->registry->get<euengine::camera_component>(g_camera_entity);

    if (ctx->input.mouse_captured)
    {
        cam.yaw += ctx->input.mouse_xrel * cam.look_speed;
        cam.pitch -= ctx->input.mouse_yrel * cam.look_speed;
        cam.pitch = glm::clamp(cam.pitch, -89.0f, 89.0f);
    }

    if (ctx->input.keyboard != nullptr)
    {
        float speed = cam.move_speed * ctx->time.delta;
        if (ctx->input.keyboard[key_lshift])
        {
            speed *= 3.0f;
        }

        glm::vec3 front = cam.front();
        glm::vec3 right = cam.right();

        if (ctx->input.keyboard[key_w]) cam.position += front * speed;
        if (ctx->input.keyboard[key_s]) cam.position -= front * speed;
        if (ctx->input.keyboard[key_a]) cam.position -= right * speed;
        if (ctx->input.keyboard[key_d]) cam.position += right * speed;
        if (ctx->input.keyboard[key_e]) cam.position.y += speed;
        if (ctx->input.keyboard[key_q]) cam.position.y -= speed;

        if (ctx->input.keyboard[key_escape])
        {
            ctx->settings->set_mouse_captured(false);
        }

        // Hotkeys with debounce
        static bool keys[10] = {};

        if (ctx->input.keyboard[key_1] && !keys[0])
        {
            g_sky_preset = 0;
            apply_sky_preset(0);
        }
        keys[0] = ctx->input.keyboard[key_1];

        if (ctx->input.keyboard[key_2] && !keys[1])
        {
            g_sky_preset = 1;
            apply_sky_preset(1);
        }
        keys[1] = ctx->input.keyboard[key_2];

        if (ctx->input.keyboard[key_3] && !keys[2])
        {
            g_sky_preset = 2;
            apply_sky_preset(2);
        }
        keys[2] = ctx->input.keyboard[key_3];

        if (ctx->input.keyboard[key_space] && !keys[3])
        {
            g_auto_rotate = !g_auto_rotate;
        }
        keys[3] = ctx->input.keyboard[key_space];

        if (ctx->input.keyboard[key_tab] && !keys[4])
        {
            g_wireframe = !g_wireframe;
        }
        keys[4] = ctx->input.keyboard[key_tab];

        if (ctx->input.keyboard[key_f11] && !keys[5])
        {
            ctx->settings->set_fullscreen(!ctx->settings->is_fullscreen());
        }
        keys[5] = ctx->input.keyboard[key_f11];

        if (ctx->input.keyboard[key_f5] && !keys[6])
        {
            scan_model_directory(g_model_dir);
        }
        keys[6] = ctx->input.keyboard[key_f5];
    }

    ctx->renderer->set_view_projection(
        cam.projection(ctx->display.aspect) * cam.view());
}

void update_animations(float time, float delta)
{
    for (auto& m : g_showcase)
    {
        if (m.rotate && g_auto_rotate)
        {
            m.transform.rotation.y += m.rotate_speed * delta;
            if (m.transform.rotation.y > 360.0f)
            {
                m.transform.rotation.y -= 360.0f;
            }
        }
        if (m.hover)
        {
            m.transform.position.y =
                m.hover_height + std::sin(time * m.hover_speed) * m.hover_amount;
        }
    }
}

// ============================================================================
// UI Panels
// ============================================================================

void ui_toolbar(euengine::engine_context* ctx)
{
    ImGuiIO& io = ImGui::GetIO();
    
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, 32));
    
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoBringToFrontOnFocus;
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 4));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 0));
    
    if (ImGui::Begin("##toolbar", nullptr, flags))
    {
        // View toggles
        if (ImGui::Button(g_show_hierarchy ? "H" : "h"))
        {
            g_show_hierarchy = !g_show_hierarchy;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Hierarchy (H)");
        
        ImGui::SameLine();
        if (ImGui::Button(g_show_properties ? "P" : "p"))
        {
            g_show_properties = !g_show_properties;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Properties (P)");
        
        ImGui::SameLine();
        if (ImGui::Button(g_show_browser ? "B" : "b"))
        {
            g_show_browser = !g_show_browser;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Browser (B)");
        
        ImGui::SameLine();
        if (ImGui::Button(g_show_music ? "M" : "m"))
        {
            g_show_music = !g_show_music;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Music (M)");
        
        ImGui::SameLine();
        ImGui::Dummy(ImVec2(20, 0));
        ImGui::SameLine();
        
        // Wireframe toggle
        if (ImGui::Checkbox("Wire", &g_wireframe))
        {
            // Updated in render
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle wireframe (Tab)");
        
        ImGui::SameLine();
        ImGui::Checkbox("Rotate", &g_auto_rotate);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Auto-rotate models (Space)");
        
        ImGui::SameLine();
        ImGui::Dummy(ImVec2(20, 0));
        ImGui::SameLine();
        
        // Sky presets
        ImGui::SetNextItemWidth(100);
        const char* presets[] = { "Dark", "Day", "Sunset", "Night" };
        if (ImGui::Combo("Sky", &g_sky_preset, presets, 4))
        {
            apply_sky_preset(g_sky_preset);
        }
        
        // Custom color
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120);
        if (ImGui::ColorEdit3("##skycolor", g_sky_color,
                              ImGuiColorEditFlags_NoInputs))
        {
            if (g_ctx->background)
            {
                g_ctx->background->r = g_sky_color[0];
                g_ctx->background->g = g_sky_color[1];
                g_ctx->background->b = g_sky_color[2];
            }
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Custom sky color");
        
        // Right side - stats
        char stats[64];
        snprintf(stats, sizeof(stats), "%.0f FPS | %zu models",
                 ctx->time.fps, g_showcase.size());
        float w = ImGui::CalcTextSize(stats).x;
        ImGui::SameLine(io.DisplaySize.x - w - 16);
        ImGui::TextColored(ImVec4(0.5f, 0.8f, 0.5f, 1), "%s", stats);
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
}

void ui_hierarchy(euengine::engine_context* ctx)
{
    if (!g_show_hierarchy) return;
    
    ImGuiIO& io = ImGui::GetIO();
    
    ImGui::SetNextWindowPos(ImVec2(8, 40), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(240, 350), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(180, 200), ImVec2(400, io.DisplaySize.y - 60));
    
    if (ImGui::Begin("Scene", &g_show_hierarchy))
    {
        // Camera section
        if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (g_camera_entity != entt::null && ctx->registry->valid(g_camera_entity))
            {
                auto& cam = ctx->registry->get<euengine::camera_component>(g_camera_entity);
                
                ImGui::Text("Position");
                ImGui::PushItemWidth(-1);
                ImGui::DragFloat3("##campos", &cam.position.x, 0.5f);
                ImGui::PopItemWidth();
                
                ImGui::Spacing();
                ImGui::SliderFloat("Speed", &cam.move_speed, 1.0f, 100.0f, "%.0f");
                ImGui::SliderFloat("FOV", &cam.fov, 30.0f, 120.0f, "%.0f");
            }
        }
        
        ImGui::Spacing();
        
        // Objects section
        if (ImGui::CollapsingHeader("Objects", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::BeginChild("##objlist", ImVec2(0, 0), false);
            
            for (std::size_t i = 0; i < g_showcase.size(); ++i)
            {
                auto& m = g_showcase[i];
                bool selected = (static_cast<int>(i) == g_selected_model);
                
                // Icon based on type
                const char* icon = "  ";
                if (m.hover) icon = "^ ";
                else if (m.rotate) icon = "* ";
                
                char label[128];
                snprintf(label, sizeof(label), "%s%s##%zu", icon, m.name.c_str(), i);
                
                if (ImGui::Selectable(label, selected))
                {
                    g_selected_model = static_cast<int>(i);
                }
                
                // Double-click to focus
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
                {
                    if (g_camera_entity != entt::null)
                    {
                        auto& cam = ctx->registry->get<euengine::camera_component>(g_camera_entity);
                        cam.position = m.transform.position + glm::vec3(0, 5, 15);
                        cam.pitch = -15.0f;
                        cam.yaw = -90.0f;
                    }
                }
            }
            
            ImGui::EndChild();
        }
    }
    ImGui::End();
}

void ui_properties(euengine::engine_context* ctx)
{
    if (!g_show_properties) return;
    
    ImGuiIO& io = ImGui::GetIO();
    
    ImGui::SetNextWindowPos(ImVec2(8, 400), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(240, 280), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(200, 150), ImVec2(400, io.DisplaySize.y - 60));
    
    if (ImGui::Begin("Properties", &g_show_properties))
    {
        if (g_selected_model >= 0 &&
            static_cast<std::size_t>(g_selected_model) < g_showcase.size())
        {
            auto& m = g_showcase[static_cast<std::size_t>(g_selected_model)];
            
            ImGui::PushID(g_selected_model);
            
            // Header
            ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "%s", m.name.c_str());
            ImGui::Separator();
            ImGui::Spacing();
            
            // Transform
            if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::Text("Position");
                ImGui::PushItemWidth(-1);
                ImGui::DragFloat3("##pos", &m.transform.position.x, 0.1f);
                ImGui::PopItemWidth();
                
                ImGui::Spacing();
                ImGui::Text("Rotation");
                ImGui::PushItemWidth(-1);
                ImGui::DragFloat3("##rot", &m.transform.rotation.x, 1.0f);
                ImGui::PopItemWidth();
                
                ImGui::Spacing();
                ImGui::Text("Scale");
                float scale = m.transform.scale.x;
                if (ImGui::SliderFloat("##scale", &scale, 0.1f, 5.0f, "%.2f"))
                {
                    m.transform.scale = glm::vec3(scale);
                }
            }
            
            // Animation
            if (ImGui::CollapsingHeader("Animation"))
            {
                ImGui::Checkbox("Auto Rotate", &m.rotate);
                if (m.rotate)
                {
                    ImGui::SliderFloat("Speed##rot", &m.rotate_speed, 0.0f, 180.0f);
                }
                
                ImGui::Spacing();
                ImGui::Checkbox("Hover", &m.hover);
                if (m.hover)
                {
                    ImGui::SliderFloat("Amount", &m.hover_amount, 0.0f, 2.0f);
                    ImGui::SliderFloat("Speed##hov", &m.hover_speed, 0.5f, 5.0f);
                }
            }
            
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            
            // Actions
            if (ImGui::Button("Remove", ImVec2(-1, 0)))
            {
                ctx->renderer->unload_model(m.handle);
                g_showcase.erase(g_showcase.begin() + g_selected_model);
                g_selected_model = -1;
            }
            
            ImGui::PopID();
        }
        else
        {
            ImGui::TextDisabled("Select an object");
        }
    }
    ImGui::End();
}

void ui_browser()
{
    if (!g_show_browser) return;
    
    ImGuiIO& io = ImGui::GetIO();
    
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 320, 40), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(310, 400), ImGuiCond_FirstUseEver);
    
    if (ImGui::Begin("Model Browser", &g_show_browser))
    {
        // Path input
        static char dir_buf[256];
        strncpy(dir_buf, g_model_dir.c_str(), sizeof(dir_buf) - 1);
        
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 60);
        if (ImGui::InputText("##dir", dir_buf, sizeof(dir_buf)))
        {
            g_model_dir = dir_buf;
        }
        ImGui::SameLine();
        if (ImGui::Button("Scan"))
        {
            scan_model_directory(g_model_dir);
        }
        
        ImGui::Text("%zu models found", g_model_files.size());
        ImGui::Separator();
        
        // File list
        ImGui::BeginChild("##files", ImVec2(0, -36), true);
        for (std::size_t i = 0; i < g_model_files.size(); ++i)
        {
            auto name = std::filesystem::path(g_model_files[i]).filename().string();
            if (ImGui::Selectable(name.c_str(), std::cmp_equal(i, g_browser_selected)))
            {
                g_browser_selected = static_cast<int>(i);
            }
        }
        ImGui::EndChild();
        
        // Load button
        bool can_load = g_browser_selected >= 0 &&
                        static_cast<std::size_t>(g_browser_selected) < g_model_files.size();
        
        if (!can_load) ImGui::BeginDisabled();
        if (ImGui::Button("Load", ImVec2(-1, 0)))
        {
            load_showcase_model(
                g_model_files[static_cast<std::size_t>(g_browser_selected)],
                { 0, 0, 0 }, true);
            g_selected_model = static_cast<int>(g_showcase.size()) - 1;
        }
        if (!can_load) ImGui::EndDisabled();
    }
    ImGui::End();
}

void ui_music(euengine::engine_context* ctx)
{
    if (!g_show_music || ctx->audio == nullptr) return;
    
    ImGuiIO& io = ImGui::GetIO();
    
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 320, 460), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(310, 180), ImGuiCond_FirstUseEver);
    
    if (ImGui::Begin("Music", &g_show_music))
    {
        // Track list
        ImGui::BeginChild("##tracks", ImVec2(0, -50), true);
        for (std::size_t i = 0; i < g_music_tracks.size(); ++i)
        {
            auto& t = g_music_tracks[i];
            bool playing = std::cmp_equal(i, g_current_track);
            
            if (playing)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 1.0f, 0.4f, 1.0f));
            }
            
            if (ImGui::Selectable(t.name.c_str(), playing))
            {
                if (t.handle == euengine::invalid_music)
                {
                    t.handle = ctx->audio->load_music(t.path);
                }
                if (t.handle != euengine::invalid_music)
                {
                    ctx->audio->play_music(t.handle, true);
                    g_current_track = static_cast<int>(i);
                }
            }
            
            if (playing)
            {
                ImGui::PopStyleColor();
            }
        }
        ImGui::EndChild();
        
        // Controls
        bool is_playing = ctx->audio->is_music_playing();
        bool is_paused = ctx->audio->is_music_paused();
        
        if (is_playing && !is_paused)
        {
            if (ImGui::Button("Pause", ImVec2(60, 0)))
            {
                ctx->audio->pause_music();
            }
        }
        else
        {
            if (ImGui::Button("Play", ImVec2(60, 0)))
            {
                ctx->audio->resume_music();
            }
        }
        
        ImGui::SameLine();
        if (ImGui::Button("Stop", ImVec2(60, 0)))
        {
            ctx->audio->stop_music();
            g_current_track = -1;
        }
        
        ImGui::SameLine();
        float vol = ctx->audio->get_music_volume();
        ImGui::SetNextItemWidth(-1);
        if (ImGui::SliderFloat("##vol", &vol, 0.0f, 1.0f, "Vol: %.2f"))
        {
            ctx->audio->set_music_volume(vol);
        }
    }
    ImGui::End();
}

void ui_statusbar(euengine::engine_context* ctx)
{
    ImGuiIO& io = ImGui::GetIO();
    
    ImGui::SetNextWindowPos(ImVec2(0, io.DisplaySize.y - 24));
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, 24));
    
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoBringToFrontOnFocus;
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 4));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.1f, 0.1f, 0.12f, 0.95f));
    
    if (ImGui::Begin("##status", nullptr, flags))
    {
        const char* help = ctx->input.mouse_captured
            ? "WASD move | QE up/down | Shift fast | 1-3 sky | Space rotate | Tab wire | ESC release"
            : "Click to capture | WASD move | 1-3 sky | Space rotate | Tab wire | F11 fullscreen";
        
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s", help);
        
        // Right side info
        char info[64];
        snprintf(info, sizeof(info), "%.1f ms | %dx%d",
                 ctx->time.delta * 1000.0f,
                 ctx->settings->get_window_width(),
                 ctx->settings->get_window_height());
        float w = ImGui::CalcTextSize(info).x;
        ImGui::SameLine(io.DisplaySize.x - w - 16);
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%s", info);
    }
    ImGui::End();
    
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

} // namespace

// ============================================================================
// Game API
// ============================================================================

GAME_API euengine::preinit_result game_preinit(euengine::preinit_settings* s)
{
    spdlog::info("=> game_preinit");
    
    s->window.title     = "euengine showcase";
    s->window.width     = 1600;
    s->window.height    = 900;
    s->window.vsync     = euengine::vsync_mode::enabled;
    s->window.resizable = true;
    s->window.high_dpi  = true;
    
    s->audio.master_volume = 0.8f;
    s->audio.music_volume  = 0.4f;
    
    s->background = euengine::clear_color::sky();
    
    return euengine::preinit_result::ok;
}

GAME_API bool game_init(euengine::engine_context* ctx)
{
    g_ctx = ctx;
    
    // Setup ImGui style
    ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx->imgui_ctx));
    setup_imgui_style();
    
    // Camera
    if (g_camera_entity != entt::null && ctx->registry->valid(g_camera_entity))
    {
        ctx->registry->destroy(g_camera_entity);
    }
    
    g_camera_entity = ctx->registry->create();
    auto& cam = ctx->registry->emplace<euengine::camera_component>(g_camera_entity);
    cam.position   = { 0, 20, 35 };
    cam.pitch      = -30;
    cam.yaw        = -90;
    cam.move_speed = 20;
    cam.look_speed = 0.12f;
    cam.fov        = 55;
    cam.far_plane  = 500;
    
    // Scene
    setup_showcase_scene();
    scan_model_directory(g_model_dir);
    scan_music_directory();
    
    ctx->renderer->set_render_mode(euengine::render_mode::textured);
    apply_sky_preset(g_sky_preset);
    
    spdlog::info("showcase initialized");
    return true;
}

GAME_API void game_shutdown()
{
    for (auto& m : g_showcase)
    {
        if (m.handle != euengine::invalid_model && g_ctx->renderer)
        {
            g_ctx->renderer->unload_model(m.handle);
        }
    }
    g_showcase.clear();
    
    for (auto& t : g_music_tracks)
    {
        if (t.handle != euengine::invalid_music && g_ctx->audio)
        {
            g_ctx->audio->unload_music(t.handle);
        }
    }
    g_music_tracks.clear();
    
    for (auto h : g_meshes)
    {
        if (h != euengine::invalid_mesh && g_ctx->renderer)
        {
            g_ctx->renderer->destroy_mesh(h);
        }
    }
    g_meshes.clear();
    
    if (g_camera_entity != entt::null && g_ctx->registry->valid(g_camera_entity))
    {
        g_ctx->registry->destroy(g_camera_entity);
        g_camera_entity = entt::null;
    }
    
    spdlog::info("showcase shutdown");
    g_ctx = nullptr;
}

GAME_API void game_update(euengine::engine_context* ctx)
{
    ctx->renderer->set_render_mode(
        g_wireframe ? euengine::render_mode::wireframe : euengine::render_mode::textured);
    
    update_camera(ctx);
    update_animations(ctx->time.elapsed, ctx->time.delta);
    
    if (ctx->imgui_ctx)
    {
        ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx->imgui_ctx));
        if (!ImGui::GetIO().WantCaptureMouse && ImGui::GetIO().MouseClicked[0])
        {
            ctx->settings->set_mouse_captured(true);
        }
    }
}

GAME_API void game_render(euengine::engine_context* ctx)
{
    for (auto h : g_meshes)
    {
        ctx->renderer->draw(h);
    }
    
    for (auto& m : g_showcase)
    {
        ctx->renderer->draw_model(m.handle, m.transform);
        
        if (&m - g_showcase.data() == g_selected_model)
        {
            ctx->renderer->draw_bounds(m.bounds, m.transform, { 0, 1, 0 });
        }
    }
}

GAME_API void game_ui(euengine::engine_context* ctx)
{
    ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx->imgui_ctx));
    
    ui_toolbar(ctx);
    ui_hierarchy(ctx);
    ui_properties(ctx);
    ui_browser();
    ui_music(ctx);
    ui_statusbar(ctx);
}
