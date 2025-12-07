#include "ui.hpp"
#include "scene.hpp"

#include <core-api/camera.hpp>
#include <imgui.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <ranges>

namespace ui
{

namespace
{

// Sanitize UTF-8 string to avoid Pango warnings
std::string sanitize_utf8(const std::string& str)
{
    std::string result;
    result.reserve(str.size());

    for (std::size_t i = 0; i < str.size(); ++i)
    {
        unsigned char c = static_cast<unsigned char>(str[i]);

        // Valid ASCII
        if (c < 0x80)
        {
            result += static_cast<char>(c);
        }
        // Valid UTF-8 continuation or start
        else if ((c & 0xE0) == 0xC0 && i + 1 < str.size())
        {
            // 2-byte sequence
            unsigned char c2 = static_cast<unsigned char>(str[i + 1]);
            if ((c2 & 0xC0) == 0x80)
            {
                result += static_cast<char>(c);
                result += static_cast<char>(c2);
                ++i;
            }
            else
            {
                result += '?'; // Invalid, replace
            }
        }
        else if ((c & 0xF0) == 0xE0 && i + 2 < str.size())
        {
            // 3-byte sequence
            unsigned char c2 = static_cast<unsigned char>(str[i + 1]);
            unsigned char c3 = static_cast<unsigned char>(str[i + 2]);
            if ((c2 & 0xC0) == 0x80 && (c3 & 0xC0) == 0x80)
            {
                result += static_cast<char>(c);
                result += static_cast<char>(c2);
                result += static_cast<char>(c3);
                i += 2;
            }
            else
            {
                result += '?';
            }
        }
        else if ((c & 0xF8) == 0xF0 && i + 3 < str.size())
        {
            // 4-byte sequence
            unsigned char c2 = static_cast<unsigned char>(str[i + 1]);
            unsigned char c3 = static_cast<unsigned char>(str[i + 2]);
            unsigned char c4 = static_cast<unsigned char>(str[i + 3]);
            if ((c2 & 0xC0) == 0x80 && (c3 & 0xC0) == 0x80 &&
                (c4 & 0xC0) == 0x80)
            {
                result += static_cast<char>(c);
                result += static_cast<char>(c2);
                result += static_cast<char>(c3);
                result += static_cast<char>(c4);
                i += 3;
            }
            else
            {
                result += '?';
            }
        }
        else
        {
            result += '?'; // Invalid byte, replace with ?
        }
    }

    return result;
}

// Modern Steam 2024+ inspired theme - clean, flat, professional
// Load a glTF/GLB scene file
void load_gltf_scene(const std::filesystem::path& path)
{
    std::string filename = sanitize_utf8(path.filename().string());
    log(2, "Loading glTF: " + filename);

    // Load model at origin with default scale
    // Engine's glTF loader handles hierarchy and transforms internally
    if (auto* m = scene::add_model(path.string(), { 0.0f, 0.0f, 0.0f }, 1.0f))
    {
        m->name = path.stem().string();
        log(2, "Loaded: " + filename);
        g_show_file_dialog = false;
        g_file_dialog_selected_file.clear();
    }
    else
    {
        log(4, "Failed to load: " + filename);
    }
}

void apply_theme()
{
    ImGuiStyle& s = ImGui::GetStyle();
    ImVec4*     c = s.Colors;

    // Modern dark palette
    const ImVec4 bg_deep   = ImVec4(0.067f, 0.067f, 0.075f, 1.0f); // #111113
    const ImVec4 bg_main   = ImVec4(0.098f, 0.098f, 0.110f, 1.0f); // #19191C
    const ImVec4 bg_panel  = ImVec4(0.129f, 0.129f, 0.145f, 1.0f); // #212125
    const ImVec4 bg_hover  = ImVec4(0.180f, 0.180f, 0.200f, 1.0f); // #2E2E33
    const ImVec4 bg_active = ImVec4(0.220f, 0.220f, 0.245f, 1.0f); // #38383E

    // Modern accent - gradient-like blue-cyan
    const ImVec4 accent   = ImVec4(0.380f, 0.680f, 0.934f, 1.0f); // #61ADEE
    const ImVec4 accent_h = ImVec4(0.480f, 0.750f, 0.970f, 1.0f); // #7AC0F8
    const ImVec4 accent_d = ImVec4(0.280f, 0.550f, 0.800f, 1.0f); // #478CCC

    // Text
    const ImVec4 text     = ImVec4(0.950f, 0.950f, 0.960f, 1.0f);
    const ImVec4 text_dim = ImVec4(0.550f, 0.550f, 0.580f, 1.0f);

    c[ImGuiCol_Text]         = text;
    c[ImGuiCol_TextDisabled] = text_dim;
    c[ImGuiCol_WindowBg]     = bg_main;
    c[ImGuiCol_ChildBg]      = bg_deep;
    c[ImGuiCol_PopupBg] = ImVec4(bg_panel.x, bg_panel.y, bg_panel.z, 0.98f);
    c[ImGuiCol_Border]  = ImVec4(0.20f, 0.20f, 0.22f, 0.50f);
    c[ImGuiCol_BorderShadow]         = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_FrameBg]              = bg_panel;
    c[ImGuiCol_FrameBgHovered]       = bg_hover;
    c[ImGuiCol_FrameBgActive]        = bg_active;
    c[ImGuiCol_TitleBg]              = bg_deep;
    c[ImGuiCol_TitleBgActive]        = bg_panel;
    c[ImGuiCol_TitleBgCollapsed]     = bg_deep;
    c[ImGuiCol_MenuBarBg]            = bg_deep;
    c[ImGuiCol_ScrollbarBg]          = bg_deep;
    c[ImGuiCol_ScrollbarGrab]        = bg_hover;
    c[ImGuiCol_ScrollbarGrabHovered] = accent_d;
    c[ImGuiCol_ScrollbarGrabActive]  = accent;
    c[ImGuiCol_CheckMark]            = accent;
    c[ImGuiCol_SliderGrab]           = accent_d;
    c[ImGuiCol_SliderGrabActive]     = accent;
    c[ImGuiCol_Button]               = bg_hover;
    c[ImGuiCol_ButtonHovered]        = accent_d;
    c[ImGuiCol_ButtonActive]         = accent;
    c[ImGuiCol_Header]               = bg_hover;
    c[ImGuiCol_HeaderHovered]        = accent_d;
    c[ImGuiCol_HeaderActive]         = accent;
    c[ImGuiCol_Separator]            = ImVec4(0.25f, 0.25f, 0.28f, 1.0f);
    c[ImGuiCol_SeparatorHovered]     = accent_d;
    c[ImGuiCol_SeparatorActive]      = accent;
    c[ImGuiCol_ResizeGrip]           = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ResizeGripHovered]    = accent_d;
    c[ImGuiCol_ResizeGripActive]     = accent;
    c[ImGuiCol_Tab]                  = bg_panel;
    c[ImGuiCol_TabHovered]           = accent_d;
    c[ImGuiCol_TabActive]            = accent;
    c[ImGuiCol_TabUnfocused]         = bg_main;
    c[ImGuiCol_TabUnfocusedActive]   = bg_hover;
    c[ImGuiCol_PlotLines]            = accent;
    c[ImGuiCol_PlotLinesHovered]     = accent_h;
    c[ImGuiCol_PlotHistogram]        = accent_d;
    c[ImGuiCol_PlotHistogramHovered] = accent;
    c[ImGuiCol_TableHeaderBg]        = bg_panel;
    c[ImGuiCol_TableBorderStrong]    = bg_active;
    c[ImGuiCol_TableBorderLight]     = bg_hover;
    c[ImGuiCol_TableRowBg]           = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_TableRowBgAlt]        = ImVec4(1, 1, 1, 0.015f);
    c[ImGuiCol_TextSelectedBg] = ImVec4(accent.x, accent.y, accent.z, 0.30f);
    c[ImGuiCol_DragDropTarget] = accent_h;
    c[ImGuiCol_NavHighlight]   = accent;
    c[ImGuiCol_NavWindowingHighlight] = text;
    c[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.05f, 0.05f, 0.06f, 0.20f);
    c[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.03f, 0.03f, 0.04f, 0.75f);

    // Modern flat metrics - Steam-like precision
    s.WindowRounding    = 6.0f;
    s.ChildRounding     = 4.0f;
    s.FrameRounding     = 3.0f;
    s.PopupRounding     = 6.0f;
    s.ScrollbarRounding = 6.0f;
    s.GrabRounding      = 3.0f;
    s.TabRounding       = 4.0f;

    // Improved spacing and alignment - professional look
    s.WindowPadding     = ImVec2(14, 14);
    s.FramePadding      = ImVec2(8, 5);
    s.ItemSpacing       = ImVec2(10, 6);
    s.ItemInnerSpacing  = ImVec2(8, 4);
    s.IndentSpacing     = 22.0f;
    s.ScrollbarSize     = 16.0f;
    s.GrabMinSize       = 14.0f;
    s.CellPadding       = ImVec2(6, 4);
    s.TouchExtraPadding = ImVec2(0, 0);

    s.WindowBorderSize = 0.0f;
    s.ChildBorderSize  = 0.0f;
    s.PopupBorderSize  = 1.0f;
    s.FrameBorderSize  = 0.0f;
    s.TabBorderSize    = 0.0f;

    // Better text alignment
    s.WindowTitleAlign         = ImVec2(0.0f, 0.5f); // Left-aligned titles
    s.WindowMenuButtonPosition = ImGuiDir_None;
    s.ColorButtonPosition      = ImGuiDir_Right;
    s.ButtonTextAlign          = ImVec2(0.5f, 0.5f);
    s.SelectableTextAlign      = ImVec2(0.0f, 0.0f);
    s.DisplaySafeAreaPadding   = ImVec2(3.0f, 3.0f);
}

void draw_menu(euengine::engine_context* ctx)
{
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12, 8));

    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Hot Reload", "F5"))
            {
                log(2, "Shader hot reload triggered");
                if (ctx->shaders)
                {
                    ctx->shaders->enable_hot_reload(false);
                    ctx->shaders->enable_hot_reload(true);
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Load Godot Scene..."))
            {
                g_show_file_dialog = true;
            }
            if (ImGui::MenuItem("Clear Scene"))
            {
                // Remove all models except keep camera
                while (!scene::g_models.empty())
                {
                    scene::remove_model(0);
                }
                log(2, "Scene cleared");
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Rescan Assets"))
            {
                scene::scan_models();
                scene::scan_audio();
                scene::scan_scenes();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4"))
                ctx->settings->request_quit();
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View"))
        {
            ImGui::MenuItem("Scene", nullptr, &g_show_hierarchy);
            ImGui::MenuItem("Inspector", nullptr, &g_show_inspector);
            ImGui::MenuItem("Asset Browser", nullptr, &g_show_browser);
            ImGui::MenuItem("Audio Player", nullptr, &g_show_audio);
            ImGui::Separator();
            ImGui::MenuItem("Engine Settings", nullptr, &g_show_engine);
            ImGui::MenuItem("Performance", nullptr, &g_show_stats);
            ImGui::MenuItem("Console", "`", &g_show_console);
            ImGui::Separator();
            if (ImGui::MenuItem("Reset Window Layout"))
            {
                // Set flag to reset all window positions on next frame
                g_reset_window_layout = true;

                // Also clear INI file for future sessions
                ImGuiIO& io = ImGui::GetIO();
                if (io.IniFilename != nullptr)
                {
                    try
                    {
                        std::filesystem::path ini_path = io.IniFilename;
                        if (std::filesystem::exists(ini_path))
                        {
                            std::filesystem::remove(ini_path);
                        }
                    }
                    catch (const std::exception& e)
                    {
                        log(4,
                            std::format("Failed to delete INI file: {}",
                                        e.what()));
                    }
                }
                log(2, "Window layout will reset on next frame");
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Render"))
        {
            ImGui::MenuItem("Wireframe Mode", "Tab", &g_wireframe);
            ImGui::MenuItem("Auto Animate", "Space", &g_auto_rotate);
            ImGui::Separator();
            if (ImGui::ColorEdit3("Background",
                                  g_sky_color,
                                  ImGuiColorEditFlags_NoInputs |
                                      ImGuiColorEditFlags_NoLabel))
                scene::apply_sky();
            ImGui::SameLine();
            ImGui::Text("Background Color");
            ImGui::EndMenu();
        }

        // Right: Stats
        auto  info      = std::format("{:.0f} FPS  |  {}x{}  |  {} objects",
                                ctx->time.fps,
                                ctx->settings->get_window_width(),
                                ctx->settings->get_window_height(),
                                scene::g_models.size());
        auto  safe_info = sanitize_utf8(info);
        float w         = ImGui::CalcTextSize(safe_info.c_str()).x;
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - w - 20);
        ImGui::TextColored(
            ImVec4(0.55f, 0.55f, 0.58f, 1.0f), "%s", safe_info.c_str());

        ImGui::EndMainMenuBar();
    }
    ImGui::PopStyleVar();
}

void draw_scene(euengine::engine_context* ctx)
{
    if (!g_show_hierarchy)
        return;

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(16, 40),
                            g_reset_window_layout ? ImGuiCond_Always
                                                  : ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(320, 600),
                             g_reset_window_layout ? ImGuiCond_Always
                                                   : ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(280, 300),
                                        ImVec2(500, io.DisplaySize.y - 60));

    if (ImGui::Begin("Scene", &g_show_hierarchy))
    {
        // Camera section
        if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (scene::g_camera != entt::null &&
                ctx->registry->valid(scene::g_camera))
            {
                auto& cam = ctx->registry->get<euengine::camera_component>(
                    scene::g_camera);

                ImGui::PushStyleColor(ImGuiCol_Text,
                                      ImVec4(0.55f, 0.55f, 0.58f, 1.0f));
                ImGui::Text("Position");
                ImGui::PopStyleColor();
                ImGui::DragFloat3("##pos", &cam.position.x, 0.2f);

                ImGui::PushStyleColor(ImGuiCol_Text,
                                      ImVec4(0.55f, 0.55f, 0.58f, 1.0f));
                ImGui::Text("Settings");
                ImGui::PopStyleColor();
                ImGui::SliderFloat("Speed", &cam.move_speed, 1.0f, 50.0f);
                ImGui::SliderFloat("FOV", &cam.fov, 30.0f, 120.0f);

                ImGui::Spacing();
                if (ImGui::Button("Reset Camera", ImVec2(-1, 28)))
                {
                    cam.position = { 0.0f, 10.0f, 25.0f };
                    cam.pitch    = -15.0f;
                    cam.yaw      = 0.0f;
                }
            }
        }

        // Environment section
        if (ImGui::CollapsingHeader("Environment",
                                    ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  ImVec4(0.55f, 0.55f, 0.58f, 1.0f));
            ImGui::Text("Colors");
            ImGui::PopStyleColor();

            if (ImGui::ColorEdit3("Sky", g_sky_color))
                scene::apply_sky();
            if (ImGui::ColorEdit3("Grid", g_grid_color))
                scene::rebuild_grid();

            ImGui::Checkbox("Origin Axis", &scene::g_show_origin);
        }

        // Objects list
        if (ImGui::CollapsingHeader("Objects", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  ImVec4(0.55f, 0.55f, 0.58f, 1.0f));
            ImGui::Text(
                "%s", std::format("{} items", scene::g_models.size()).c_str());
            ImGui::PopStyleColor();

            // Filter/search for objects
            static char obj_filter[128] = {};
            ImGui::SetNextItemWidth(-1);
            ImGui::InputTextWithHint("##obj_filter",
                                     "Search objects...",
                                     obj_filter,
                                     sizeof(obj_filter));

            // Clear selection button
            if (scene::g_selected >= 0)
            {
                ImGui::SameLine();
                if (ImGui::SmallButton("Clear"))
                    scene::g_selected = -1;
            }

            ImGui::Spacing();
            ImGui::BeginChild("##objs", ImVec2(0, -1), true);

            for (std::size_t i = 0; i < scene::g_models.size(); ++i)
            {
                auto& m = scene::g_models[i];

                // Apply filter
                if (obj_filter[0] != '\0' &&
                    m.name.find(obj_filter) == std::string::npos)
                    continue;

                bool sel = (static_cast<int>(i) == scene::g_selected);

                // Type indicators with icons
                ImVec4      col  = ImVec4(0.55f, 0.55f, 0.58f, 1.0f);
                const char* icon = "";
                if (m.moving)
                {
                    col  = ImVec4(0.40f, 0.80f, 0.50f, 1.0f);
                    icon = "> ";
                }
                else if (m.hover)
                {
                    col  = ImVec4(0.40f, 0.80f, 0.95f, 1.0f);
                    icon = "^ ";
                }
                else if (m.animate)
                {
                    col  = ImVec4(0.95f, 0.75f, 0.30f, 1.0f);
                    icon = "~ ";
                }

                if (sel)
                {
                    ImGui::PushStyleColor(ImGuiCol_Header,
                                          ImVec4(0.28f, 0.55f, 0.80f, 0.6f));
                    ImGui::PushStyleColor(ImGuiCol_Text,
                                          ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
                }
                else
                {
                    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
                    ImGui::PushStyleColor(ImGuiCol_Text, col);
                }

                auto safe_name = sanitize_utf8(m.name);
                auto label     = std::format("{}{}", icon, safe_name);
                if (ImGui::Selectable(label.c_str(), sel))
                    scene::g_selected = static_cast<int>(i);

                ImGui::PopStyleColor(2);

                // Tooltip with position info
                if (ImGui::IsItemHovered())
                {
                    ImGui::BeginTooltip();
                    ImGui::Text("Position: %.1f, %.1f, %.1f",
                                m.transform.position.x,
                                m.transform.position.y,
                                m.transform.position.z);
                    ImGui::EndTooltip();
                }

                // Double-click to focus
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0) &&
                    scene::g_camera != entt::null)
                {
                    auto& cam = ctx->registry->get<euengine::camera_component>(
                        scene::g_camera);
                    cam.position = m.transform.position + glm::vec3(0, 2, 5);
                    cam.pitch    = -15.0f;
                    cam.yaw      = -90.0f;
                }
            }
            ImGui::EndChild();
        }
    }
    ImGui::End();
}

void draw_inspector()
{
    if (!g_show_inspector)
        return;

    ImGuiIO& io = ImGui::GetIO();
    // Position below Scene window (Scene is at y=40, height=600, so start at
    // 650)
    ImGui::SetNextWindowPos(ImVec2(16, 650),
                            g_reset_window_layout ? ImGuiCond_Always
                                                  : ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(320, 200),
                             g_reset_window_layout ? ImGuiCond_Always
                                                   : ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(280, 150),
                                        ImVec2(500, io.DisplaySize.y - 60));

    if (ImGui::Begin("Inspector", &g_show_inspector))
    {
        if (scene::g_selected >= 0 &&
            static_cast<std::size_t>(scene::g_selected) <
                scene::g_models.size())
        {
            auto& m =
                scene::g_models[static_cast<std::size_t>(scene::g_selected)];

            // Header
            auto safe_name = sanitize_utf8(m.name);
            auto safe_path = sanitize_utf8(
                std::filesystem::path(m.path).filename().string());
            ImGui::TextColored(
                ImVec4(0.38f, 0.68f, 0.93f, 1.0f), "%s", safe_name.c_str());
            ImGui::TextColored(
                ImVec4(0.55f, 0.55f, 0.58f, 1.0f), "%s", safe_path.c_str());
            ImGui::Separator();

            // Transform
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  ImVec4(0.55f, 0.55f, 0.58f, 1.0f));
            ImGui::Text("Transform");
            ImGui::PopStyleColor();

            if (ImGui::DragFloat3("Position", &m.transform.position.x, 0.05f))
            {
                // Apply grid snapping if enabled
                if (g_grid_snap && g_snap_size > 0.0f)
                {
                    m.transform.position.x =
                        std::round(m.transform.position.x / g_snap_size) *
                        g_snap_size;
                    m.transform.position.y =
                        std::round(m.transform.position.y / g_snap_size) *
                        g_snap_size;
                    m.transform.position.z =
                        std::round(m.transform.position.z / g_snap_size) *
                        g_snap_size;
                }
            }
            ImGui::DragFloat3("Rotation", &m.transform.rotation.x, 0.5f);

            float sc = m.transform.scale.x;
            if (ImGui::SliderFloat("Scale", &sc, 0.01f, 0.5f))
                m.transform.scale = glm::vec3(sc);

            ImGui::Separator();

            // Color tint (especially for duck)
            if (m.name.find("duck") != std::string::npos || m.name == "duck")
            {
                ImGui::PushStyleColor(ImGuiCol_Text,
                                      ImVec4(0.55f, 0.55f, 0.58f, 1.0f));
                ImGui::Text("Color Tint");
                ImGui::PopStyleColor();

                float color[3] = { m.color_tint.r,
                                   m.color_tint.g,
                                   m.color_tint.b };
                if (ImGui::ColorEdit3("##tint", color))
                {
                    m.color_tint = glm::vec3(color[0], color[1], color[2]);
                }

                ImGui::Separator();
            }

            // Animation
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  ImVec4(0.55f, 0.55f, 0.58f, 1.0f));
            ImGui::Text("Animation");
            ImGui::PopStyleColor();

            ImGui::Checkbox("Rotate", &m.animate);
            if (m.animate)
            {
                ImGui::SameLine();
                ImGui::SetNextItemWidth(100);
                ImGui::SliderFloat(
                    "##speed", &m.anim_speed, 0.0f, 100.0f, "%.0f deg/s");
            }
            ImGui::Checkbox("Hover", &m.hover);

            ImGui::Separator();

            // Actions
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  ImVec4(0.55f, 0.55f, 0.58f, 1.0f));
            ImGui::Text("Actions");
            ImGui::PopStyleColor();

            // Duplicate button
            ImGui::PushStyleColor(ImGuiCol_Button,
                                  ImVec4(0.28f, 0.55f, 0.80f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                                  ImVec4(0.35f, 0.65f, 0.90f, 1.0f));
            if (ImGui::Button("Duplicate", ImVec2(-1, 28)))
                scene::duplicate_model(scene::g_selected);
            ImGui::PopStyleColor(2);

            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Button,
                                  ImVec4(0.70f, 0.25f, 0.25f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                                  ImVec4(0.85f, 0.35f, 0.35f, 1.0f));
            if (ImGui::Button("Delete Object", ImVec2(-1, 32)))
                scene::remove_model(scene::g_selected);
            ImGui::PopStyleColor(2);
        }
        else
        {
            ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.58f, 1.0f),
                               "Select an object to inspect");
        }
    }
    ImGui::End();
}

void draw_browser()
{
    if (!g_show_browser)
        return;

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 300, 40),
                            g_reset_window_layout ? ImGuiCond_Always
                                                  : ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(284, 360),
                             g_reset_window_layout ? ImGuiCond_Always
                                                   : ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(250, 200),
                                        ImVec2(450, io.DisplaySize.y - 60));

    if (ImGui::Begin("Asset Browser", &g_show_browser))
    {
        if (ImGui::Button("Refresh", ImVec2(80, 0)))
            scene::scan_models();
        ImGui::SameLine();

        std::size_t visible_count =
            g_browser_filter.empty()
                ? scene::g_model_files.size()
                : static_cast<std::size_t>(std::ranges::count_if(
                      scene::g_model_files,
                      [](const auto& f)
                      {
                          auto name =
                              std::filesystem::path(f).filename().string();
                          return name.find(g_browser_filter) !=
                                 std::string::npos;
                      }));
        ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.58f, 1.0f),
                           "%s",
                           std::format("{} models", visible_count).c_str());

        ImGui::Separator();

        // Search filter
        ImGui::SetNextItemWidth(-1);
        char filter_buf[256] = {};
        std::strncpy(
            filter_buf, g_browser_filter.c_str(), sizeof(filter_buf) - 1);
        if (ImGui::InputTextWithHint("##browser_filter",
                                     "Search models...",
                                     filter_buf,
                                     sizeof(filter_buf)))
            g_browser_filter = filter_buf;

        ImGui::Spacing();

        ImGui::BeginChild("##list", ImVec2(0, -40), true);
        for (std::size_t i = 0; i < scene::g_model_files.size(); ++i)
        {
            auto name = std::filesystem::path(scene::g_model_files[i])
                            .filename()
                            .string();

            // Apply filter
            if (!g_browser_filter.empty() &&
                name.find(g_browser_filter) == std::string::npos)
                continue;

            auto safe_name = sanitize_utf8(name);
            bool sel       = std::cmp_equal(i, scene::g_browser_sel);
            if (ImGui::Selectable(safe_name.c_str(), sel))
                scene::g_browser_sel = static_cast<int>(i);
        }
        ImGui::EndChild();

        bool ok = scene::g_browser_sel >= 0 &&
                  static_cast<std::size_t>(scene::g_browser_sel) <
                      scene::g_model_files.size();

        ImGui::PushStyleColor(ImGuiCol_Button,
                              ok ? ImVec4(0.28f, 0.55f, 0.80f, 1.0f)
                                 : ImVec4(0.2f, 0.2f, 0.22f, 1.0f));
        if (!ok)
            ImGui::BeginDisabled();
        if (ImGui::Button("Add to Scene", ImVec2(-1, 32)))
        {
            auto* m =
                scene::add_model(scene::g_model_files[static_cast<std::size_t>(
                                     scene::g_browser_sel)],
                                 { 0, 0, 6 },
                                 0.1f);
            if (m)
                m->animate = true;
            scene::g_selected = static_cast<int>(scene::g_models.size()) - 1;
        }
        if (!ok)
            ImGui::EndDisabled();
        ImGui::PopStyleColor();
    }
    ImGui::End();
}

void draw_audio(euengine::engine_context* ctx)
{
    if (!g_show_audio || !ctx->audio)
        return;

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 300, 420),
                            g_reset_window_layout ? ImGuiCond_Always
                                                  : ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(284, 240),
                             g_reset_window_layout ? ImGuiCond_Always
                                                   : ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(250, 150),
                                        ImVec2(450, io.DisplaySize.y - 60));

    if (ImGui::Begin("Audio Player", &g_show_audio))
    {
        // Search filter
        ImGui::SetNextItemWidth(-1);
        char filter_buf[256] = {};
        std::strncpy(
            filter_buf, g_audio_filter.c_str(), sizeof(filter_buf) - 1);
        if (ImGui::InputTextWithHint("##audio_filter",
                                     "Search audio...",
                                     filter_buf,
                                     sizeof(filter_buf)))
            g_audio_filter = filter_buf;

        ImGui::Spacing();

        // Track list
        ImGui::BeginChild("##tracks", ImVec2(0, -50), true);
        for (std::size_t i = 0; i < scene::g_audio.size(); ++i)
        {
            auto& t = scene::g_audio[i];

            // Apply filter
            if (!g_audio_filter.empty() &&
                t.name.find(g_audio_filter) == std::string::npos)
                continue;

            bool playing = std::cmp_equal(i, scene::g_playing);

            ImVec4 col = ImVec4(0.85f, 0.85f, 0.88f, 1.0f);
            if (playing)
                col = ImVec4(0.40f, 0.90f, 0.55f, 1.0f);
            else if (t.is_sfx)
                col = ImVec4(0.40f, 0.75f, 0.95f, 1.0f);

            ImGui::PushStyleColor(ImGuiCol_Text, col);
            auto safe_name = sanitize_utf8(t.name);
            if (ImGui::Selectable(safe_name.c_str(), playing))
            {
                if (t.handle == euengine::invalid_music)
                    t.handle = ctx->audio->load_music(t.path);
                if (t.handle != euengine::invalid_music)
                {
                    ctx->audio->play_music(t.handle, !t.is_sfx);
                    scene::g_playing = static_cast<int>(i);
                    ctx->audio->set_music_volume(g_volume / 100.0f);
                }
            }
            ImGui::PopStyleColor();
        }
        ImGui::EndChild();

        // Controls
        bool playing = ctx->audio->is_music_playing();
        bool paused  = ctx->audio->is_music_paused();

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 6));

        float bw = 60.0f;
        if (playing && !paused)
        {
            if (ImGui::Button("Pause", ImVec2(bw, 28)))
                ctx->audio->pause_music();
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Button,
                                  ImVec4(0.28f, 0.55f, 0.80f, 1.0f));
            if (ImGui::Button("Play", ImVec2(bw, 28)))
                ctx->audio->resume_music();
            ImGui::PopStyleColor();
        }
        ImGui::SameLine();
        if (ImGui::Button("Stop", ImVec2(bw, 28)))
        {
            ctx->audio->stop_music();
            scene::g_playing = -1;
        }
        ImGui::SameLine();

        // Volume
        ImGui::SetNextItemWidth(-1);
        if (ImGui::SliderFloat("##vol", &g_volume, 0.0f, 100.0f, "Vol: %.0f%%"))
            ctx->audio->set_music_volume(g_volume / 100.0f);

        ImGui::PopStyleVar();
    }
    ImGui::End();
}

void draw_engine(euengine::engine_context* ctx)
{
    if (!g_show_engine)
        return;

    ImGuiIO& io = ImGui::GetIO();
    // Position above Performance Metrics (which is at bottom-right)
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 420, 40),
                            g_reset_window_layout ? ImGuiCond_Always
                                                  : ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, io.DisplaySize.y - 360),
                             g_reset_window_layout ? ImGuiCond_Always
                                                   : ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(350, 400),
                                        ImVec2(600, io.DisplaySize.y - 360));

    if (ImGui::Begin("Engine Settings", &g_show_engine))
    {
        // Renderer Info
        ImGui::TextColored(ImVec4(0.38f, 0.68f, 0.93f, 1.0f), "Renderer");
        ImGui::Separator();

        ImGui::Text("GPU: %s",
                    sanitize_utf8(std::string(ctx->settings->get_gpu_driver()))
                        .c_str());
        ImGui::Text("Resolution: %d x %d",
                    ctx->settings->get_window_width(),
                    ctx->settings->get_window_height());

        bool fs = ctx->settings->is_fullscreen();
        if (ImGui::Checkbox("Fullscreen (F11)", &fs))
            ctx->settings->set_fullscreen(fs);

        ImGui::Spacing();

        // VSync
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.55f, 0.58f, 1.0f));
        ImGui::Text("VSync Mode");
        ImGui::PopStyleColor();
        ImGui::Spacing();

        int vs = static_cast<int>(ctx->settings->get_vsync());
        if (ImGui::RadioButton("Enabled", vs == 1))
            ctx->settings->set_vsync(euengine::vsync_mode::enabled);
        ImGui::SameLine();
        if (ImGui::RadioButton("Adaptive", vs == 2))
            ctx->settings->set_vsync(euengine::vsync_mode::adaptive);

        // If somehow disabled, force to enabled
        if (vs == 0)
            ctx->settings->set_vsync(euengine::vsync_mode::enabled);

        ImGui::Spacing();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Anti-Aliasing Section
        ImGui::TextColored(ImVec4(0.38f, 0.68f, 0.93f, 1.0f), "Anti-Aliasing");
        ImGui::Separator();

        // MSAA
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.55f, 0.58f, 1.0f));
        ImGui::Text("MSAA");
        ImGui::PopStyleColor();
        ImGui::Spacing();

        int msaa = static_cast<int>(ctx->settings->get_msaa());

        // Check GPU support for each MSAA level
        bool msaa2_ok =
            ctx->settings->is_msaa_supported(euengine::msaa_samples::x2);
        bool msaa4_ok =
            ctx->settings->is_msaa_supported(euengine::msaa_samples::x4);
        bool msaa8_ok =
            ctx->settings->is_msaa_supported(euengine::msaa_samples::x8);
        bool msaa16_ok =
            ctx->settings->is_msaa_supported(euengine::msaa_samples::x16);

        if (ImGui::RadioButton("Off", msaa == 1))
            ctx->settings->set_msaa(euengine::msaa_samples::none);
        ImGui::SameLine();

        ImGui::BeginDisabled(!msaa2_ok);
        if (ImGui::RadioButton("2x", msaa == 2))
            ctx->settings->set_msaa(euengine::msaa_samples::x2);
        ImGui::EndDisabled();
        ImGui::SameLine();

        ImGui::BeginDisabled(!msaa4_ok);
        if (ImGui::RadioButton("4x", msaa == 4))
            ctx->settings->set_msaa(euengine::msaa_samples::x4);
        ImGui::EndDisabled();
        ImGui::SameLine();

        ImGui::BeginDisabled(!msaa8_ok);
        if (ImGui::RadioButton("8x", msaa == 8))
            ctx->settings->set_msaa(euengine::msaa_samples::x8);
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::BeginDisabled(!msaa16_ok);
        if (ImGui::RadioButton("16x*", msaa == 16))
            ctx->settings->set_msaa(euengine::msaa_samples::x16);
        ImGui::EndDisabled();
        if (msaa == 16)
        {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.30f, 1.0f), "(uses 8x)");
        }
        ImGui::Spacing();

        // FXAA (post-processing)
        bool fxaa = ctx->settings->is_fxaa_enabled();
        if (ImGui::Checkbox("FXAA (Post-Process)", &fxaa))
            ctx->settings->set_fxaa_enabled(fxaa);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(
                "Fast Approximate Anti-Aliasing (post-processing)");

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Quality Settings
        ImGui::TextColored(ImVec4(0.38f, 0.68f, 0.93f, 1.0f), "Quality");
        ImGui::Separator();

        // Render Scale
        float render_scale = ctx->settings->get_render_scale();
        if (ImGui::SliderFloat(
                "Render Scale", &render_scale, 0.25f, 4.0f, "%.2fx"))
            ctx->settings->set_render_scale(render_scale);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Internal resolution multiplier (0.25x = "
                              "quarter, 1.0x = native, 4.0x = supersampling)");

        // Texture Quality
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.55f, 0.58f, 1.0f));
        ImGui::Text("Texture Filtering");
        ImGui::PopStyleColor();
        ImGui::Spacing();

        int tex_filter = static_cast<int>(ctx->settings->get_texture_filter());
        if (ImGui::RadioButton("Nearest", tex_filter == 0))
            ctx->settings->set_texture_filter(
                euengine::i_engine_settings::texture_filter::nearest);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(
                "Pixelated, fastest - applies to newly loaded textures");
        ImGui::SameLine();
        if (ImGui::RadioButton("Linear", tex_filter == 1))
            ctx->settings->set_texture_filter(
                euengine::i_engine_settings::texture_filter::linear);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(
                "Smooth, good quality - applies to newly loaded textures");
        ImGui::SameLine();
        if (ImGui::RadioButton("Trilinear", tex_filter == 2))
            ctx->settings->set_texture_filter(
                euengine::i_engine_settings::texture_filter::trilinear);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(
                "Best quality with mipmaps - applies to newly loaded textures");

        // Max Anisotropy
        float max_aniso = ctx->settings->get_max_anisotropy();
        if (ImGui::SliderFloat(
                "Anisotropic Filtering", &max_aniso, 1.0f, 16.0f, "%.0fx"))
            ctx->settings->set_max_anisotropy(max_aniso);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(
                "Improves texture quality at oblique angles (1x-16x)");

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Performance Settings
        ImGui::TextColored(ImVec4(0.38f, 0.68f, 0.93f, 1.0f), "Performance");
        ImGui::Separator();

        // Frame Buffering
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.55f, 0.58f, 1.0f));
        ImGui::Text("Frame Buffering");
        ImGui::PopStyleColor();
        ImGui::Spacing();

        int frames = static_cast<int>(ctx->settings->get_frames_in_flight());
        if (ImGui::RadioButton("Single (1)", frames == 1))
            ctx->settings->set_frames_in_flight(1);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Lowest latency, may cause stuttering");
        ImGui::SameLine();
        if (ImGui::RadioButton("Double (2)", frames == 2))
            ctx->settings->set_frames_in_flight(2);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Balanced latency and smoothness (default)");
        ImGui::SameLine();
        if (ImGui::RadioButton("Triple (3)", frames == 3))
            ctx->settings->set_frames_in_flight(3);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Smoothest, higher latency");

        // Render Distance
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.55f, 0.58f, 1.0f));
        ImGui::Text("Render Distance");
        ImGui::PopStyleColor();
        ImGui::Spacing();

        float render_dist = ctx->settings->get_render_distance();
        if (ImGui::SliderFloat(
                "##render_dist", &render_dist, 10.0f, 10000.0f, "%.0f units"))
            ctx->settings->set_render_distance(render_dist);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Maximum rendering distance (far plane). Objects "
                              "beyond this distance are not rendered.");

        ImGui::Spacing();

        // Post-Processing settings
        ImGui::TextColored(ImVec4(0.38f, 0.68f, 0.93f, 1.0f),
                           "Post-Processing");
        ImGui::Separator();

        float gamma = ctx->settings->get_gamma();
        if (ImGui::SliderFloat("Gamma", &gamma, 1.0f, 3.0f, "%.2f"))
            ctx->settings->set_gamma(gamma);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Display gamma correction (default: 2.2)");

        float brightness = ctx->settings->get_brightness();
        if (ImGui::SliderFloat("Brightness", &brightness, -1.0f, 1.0f, "%.2f"))
            ctx->settings->set_brightness(brightness);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Brightness adjustment (default: 0)");

        float contrast = ctx->settings->get_contrast();
        if (ImGui::SliderFloat("Contrast", &contrast, 0.5f, 2.0f, "%.2f"))
            ctx->settings->set_contrast(contrast);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Contrast adjustment (default: 1.0)");

        float saturation = ctx->settings->get_saturation();
        if (ImGui::SliderFloat("Saturation", &saturation, 0.0f, 2.0f, "%.2f"))
            ctx->settings->set_saturation(saturation);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Color saturation (0.0 = grayscale, 1.0 = "
                              "normal, 2.0 = vibrant)");

        float vignette = ctx->settings->get_vignette();
        if (ImGui::SliderFloat("Vignette", &vignette, 0.0f, 1.0f, "%.2f"))
            ctx->settings->set_vignette(vignette);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Darken screen edges (0.0 = off, 1.0 = maximum)");

        if (ImGui::Button("Reset Post-Processing", ImVec2(-1, 0)))
        {
            ctx->settings->set_gamma(2.2f);
            ctx->settings->set_brightness(0.0f);
            ctx->settings->set_contrast(1.0f);
            ctx->settings->set_saturation(1.0f);
            ctx->settings->set_vignette(0.0f);
        }

        ImGui::Spacing();

        // Shaders
        ImGui::TextColored(ImVec4(0.38f, 0.68f, 0.93f, 1.0f), "Shaders");
        ImGui::Separator();

        if (ctx->shaders)
        {
            bool hot = ctx->shaders->hot_reload_enabled();
            if (ImGui::Checkbox("Enable Hot Reload", &hot))
                ctx->shaders->enable_hot_reload(hot);

            if (ImGui::Button("Reload Now (F5)", ImVec2(-1, 28)))
            {
                log(2, "Shader hot reload triggered");
                ctx->shaders->enable_hot_reload(false);
                ctx->shaders->enable_hot_reload(true);
            }
        }

        ImGui::Spacing();

        // Game Module
        ImGui::TextColored(ImVec4(0.38f, 0.68f, 0.93f, 1.0f), "Game Module");
        ImGui::Separator();

        if (!scene::g_lib_path.empty())
        {
            auto name = sanitize_utf8(
                std::filesystem::path(scene::g_lib_path).filename().string());
            ImGui::Text("%s", std::format("File: {}", name).c_str());

            float kb = static_cast<float>(scene::g_lib_size) / 1024.0f;
            float mb = kb / 1024.0f;
            if (mb >= 1.0f)
                ImGui::Text("%s", std::format("Size: {:.2f} MB", mb).c_str());
            else
                ImGui::Text("%s", std::format("Size: {:.1f} KB", kb).c_str());

            // File timestamp
            std::filesystem::path p(scene::g_lib_path);
            if (std::filesystem::exists(p))
            {
                auto ftime = std::filesystem::last_write_time(p);
                auto sctp  = std::chrono::time_point_cast<
                     std::chrono::system_clock::duration>(
                    ftime - std::filesystem::file_time_type::clock::now() +
                    std::chrono::system_clock::now());
                auto time_t = std::chrono::system_clock::to_time_t(sctp);
                char buf[64];
                std::strftime(buf,
                              sizeof(buf),
                              "%Y-%m-%d %H:%M:%S",
                              std::localtime(&time_t));
                ImGui::Text("Modified: %s", buf);
            }

            auto safe_path = sanitize_utf8(scene::g_lib_path);
            ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.58f, 1.0f),
                               "Path: %s",
                               safe_path.c_str());

            ImGui::Spacing();

            // Hot reload button
            if (ImGui::Button("Hot Reload Module (F5)", ImVec2(-1, 32)))
            {
                log(2, "Game module hot reload triggered");
                if (ctx->settings)
                {
                    if (ctx->settings->reload_game())
                    {
                        log(2, "Game module reloaded successfully");
                    }
                    else
                    {
                        log(4, "Failed to reload game module");
                    }
                }
            }
        }
        else
        {
            ImGui::Text("Using built-in game module");
        }
    }
    ImGui::End();
}

void draw_stats(euengine::engine_context* ctx)
{
    if (!g_show_stats)
        return;

    ImGuiIO& io = ImGui::GetIO();

    // Enhanced floating overlay - bottom right, larger
    float w = 380.0f;
    float h = 280.0f;
    ImGui::SetNextWindowPos(
        ImVec2(io.DisplaySize.x - w - 16, io.DisplaySize.y - h - 40),
        g_reset_window_layout ? ImGuiCond_Always : ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(w, h),
                             g_reset_window_layout ? ImGuiCond_Always
                                                   : ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.92f);

    // Use title bar like engine settings - allows built-in close button
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoSavedSettings;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 10));

    // Use built-in close button like engine settings window
    if (ImGui::Begin("Performance Metrics", &g_show_stats, flags))
    {
        ImGui::Separator();

        // Current stats
        auto perf_text = std::format("{:.2f} ms  |  {:.0f} FPS",
                                     ctx->time.delta * 1000.0f,
                                     ctx->time.fps);
        ImGui::Text("%s", perf_text.c_str());

        // FPS stats
        if (scene::g_max_fps > 0.0f)
        {
            auto fps_stats =
                std::format("FPS: Min {:.0f} | Avg {:.0f} | Max {:.0f}",
                            scene::g_min_fps,
                            scene::g_avg_fps,
                            scene::g_max_fps);
            ImGui::TextColored(
                ImVec4(0.55f, 0.55f, 0.58f, 1.0f), "%s", fps_stats.c_str());
        }

        auto frame_text = std::format("Frame {}  |  {:.1f}s elapsed",
                                      ctx->time.frame_count,
                                      ctx->time.elapsed);
        ImGui::TextColored(
            ImVec4(0.55f, 0.55f, 0.58f, 1.0f), "%s", frame_text.c_str());

        // Render stats
        auto render_text = std::format("Draw Calls: {}  |  Triangles: ~{}",
                                       scene::g_draw_calls,
                                       scene::g_triangles);
        ImGui::TextColored(
            ImVec4(0.55f, 0.55f, 0.58f, 1.0f), "%s", render_text.c_str());

        ImGui::Spacing();

        // Performance graphs using ImGui PlotLines
        constexpr int size = 300;
        int           idx  = scene::g_frame_idx;
        float         reordered_times[300];
        float         reordered_fps[300];

        // Reorder circular buffers for plotting
        for (int i = 0; i < size; ++i)
        {
            int src_idx = (idx + i) % size;
            if (src_idx >= 0 && src_idx < size)
            {
                reordered_times[i] = scene::g_frame_times[src_idx];
                reordered_fps[i]   = scene::g_fps_history[src_idx];
            }
            else
            {
                reordered_times[i] = 0.0f;
                reordered_fps[i]   = 0.0f;
            }
        }

        // Frame time graph with legend
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.55f, 0.58f, 1.0f));
        ImGui::Text("Frame Time (ms)");
        ImGui::PopStyleColor();
        ImGui::PushStyleColor(ImGuiCol_PlotLines,
                              ImVec4(0.38f, 0.68f, 0.93f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBg,
                              ImVec4(0.067f, 0.067f, 0.075f, 1.0f));
        ImGui::PlotLines("##frame_time",
                         reordered_times,
                         size,
                         0,
                         nullptr,
                         0.0f,
                         33.3f,
                         ImVec2(-1, 60));
        ImGui::PopStyleColor(2);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.45f, 0.48f, 1.0f));
        ImGui::Text("  0 ms                                16.67 ms (60 FPS)");
        ImGui::PopStyleColor();

        ImGui::Spacing();

        // FPS history graph with legend
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.55f, 0.58f, 1.0f));
        ImGui::Text("FPS History");
        ImGui::PopStyleColor();
        ImGui::PushStyleColor(ImGuiCol_PlotLines,
                              ImVec4(0.40f, 0.80f, 0.50f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBg,
                              ImVec4(0.067f, 0.067f, 0.075f, 1.0f));
        ImGui::PlotLines("##fps_history",
                         reordered_fps,
                         size,
                         0,
                         nullptr,
                         0.0f,
                         120.0f,
                         ImVec2(-1, 60));
        ImGui::PopStyleColor(2);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.45f, 0.48f, 1.0f));
        ImGui::Text("  0 FPS                               60 FPS");
        ImGui::PopStyleColor();
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
}

void draw_console()
{
    if (!g_show_console)
        return;

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(16, io.DisplaySize.y - 280),
                            g_reset_window_layout ? ImGuiCond_Always
                                                  : ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(600, 240),
                             g_reset_window_layout ? ImGuiCond_Always
                                                   : ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(400, 120),
                                        ImVec2(io.DisplaySize.x - 32, 500));

    if (ImGui::Begin("Console", &g_show_console))
    {
        // Toolbar
        if (ImGui::Button("Clear", ImVec2(60, 0)))
            log_clear();
        ImGui::SameLine();

        // Filter input
        ImGui::SetNextItemWidth(200);
        char filter_buf[256] = {};
        std::strncpy(
            filter_buf, g_console_filter.c_str(), sizeof(filter_buf) - 1);
        if (ImGui::InputTextWithHint(
                "##filter", "Filter...", filter_buf, sizeof(filter_buf)))
            g_console_filter = filter_buf;
        ImGui::SameLine();

        std::size_t entry_count =
            g_console_filter.empty()
                ? g_log.size()
                : static_cast<std::size_t>(std::ranges::count_if(
                      g_log,
                      [](const auto& e)
                      {
                          return e.message.find(g_console_filter) !=
                                 std::string::npos;
                      }));
        ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.58f, 1.0f),
                           "%s",
                           std::format("{} entries", entry_count).c_str());

        ImGui::Separator();

        // Log view
        ImGui::BeginChild(
            "##log", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);

        bool should_scroll = false;
        for (const auto& e : g_log)
        {
            // Apply filter
            if (!g_console_filter.empty() &&
                e.message.find(g_console_filter) == std::string::npos)
                continue;

            // Timestamp
            auto time_str = std::format("[{:.1f}]", e.time);
            ImGui::TextColored(
                ImVec4(0.45f, 0.45f, 0.48f, 1.0f), "%s", time_str.c_str());
            ImGui::SameLine();

            // Level tag and color
            ImVec4      col;
            const char* tag;
            switch (e.level)
            {
                case 0:
                    col = ImVec4(0.50f, 0.50f, 0.53f, 1.0f);
                    tag = "TRACE";
                    break;
                case 1:
                    col = ImVec4(0.60f, 0.60f, 0.65f, 1.0f);
                    tag = "DEBUG";
                    break;
                case 2:
                    col = ImVec4(0.38f, 0.68f, 0.93f, 1.0f);
                    tag = "INFO";
                    break;
                case 3:
                    col = ImVec4(0.95f, 0.75f, 0.30f, 1.0f);
                    tag = "WARN";
                    break;
                case 4:
                    col = ImVec4(0.95f, 0.40f, 0.40f, 1.0f);
                    tag = "ERROR";
                    break;
                default:
                    col = ImVec4(0.70f, 0.70f, 0.70f, 1.0f);
                    tag = "???";
                    break;
            }

            ImGui::TextColored(col, "[%s]", tag);
            ImGui::SameLine();
            auto safe_msg = sanitize_utf8(e.message);
            ImGui::TextWrapped("%s", safe_msg.c_str());

            should_scroll = true;
        }

        if (g_log_scroll && should_scroll)
        {
            ImGui::SetScrollHereY(1.0f);
            g_log_scroll = false;
        }

        ImGui::EndChild();
    }
    ImGui::End();
}

void draw_statusbar(euengine::engine_context* ctx)
{
    ImGuiIO& io = ImGui::GetIO();

    float h = 28.0f;
    ImGui::SetNextWindowPos(ImVec2(0, io.DisplaySize.y - h));
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, h));

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoSavedSettings;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 6));
    ImGui::PushStyleColor(ImGuiCol_WindowBg,
                          ImVec4(0.067f, 0.067f, 0.075f, 1.0f));

    if (ImGui::Begin("##bar", nullptr, flags))
    {
        const char* help =
            ctx->input.mouse_captured
                ? "WASD Move | QE Up/Down | Shift Speed | Space Animate | Tab "
                  "Wireframe | F5 Reload | ESC Release"
                : "Click to capture mouse | F5 Reload | F11 Fullscreen | ` "
                  "Console";

        ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.58f, 1.0f), "%s", help);
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

} // namespace

void log(int level, const std::string& msg)
{
    g_log.push_back({ msg, level, g_time });
    if (g_log.size() > g_log_max)
        g_log.pop_front();
    g_log_scroll = true;
}

void log_clear()
{
    g_log.clear();
}

void init()
{
    apply_theme();
}

void draw(euengine::engine_context* ctx)
{
    ImGuiIO& io = ImGui::GetIO();

    // When camera is focused (mouse captured), disable UI mouse interaction
    // This prevents UI from interfering with camera control
    if (ctx->input.mouse_captured)
    {
        io.WantCaptureMouse = false;
    }

    draw_menu(ctx);
    draw_scene(ctx);
    draw_inspector();
    draw_browser();
    draw_audio(ctx);
    draw_engine(ctx);
    draw_stats(ctx);
    draw_console();
    draw_file_dialog();
    draw_statusbar(ctx);

    // Reset window layout flag after all windows have been drawn
    if (g_reset_window_layout)
    {
        g_reset_window_layout = false;
    }

    // Keep UI from capturing mouse when camera is focused
    if (ctx->input.mouse_captured)
    {
        io.WantCaptureMouse = false;
    }
}

void draw_file_dialog()
{
    if (!g_show_file_dialog)
        return;

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(
        ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
        ImGuiCond_Appearing,
        ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(650, 450), ImGuiCond_Appearing);

    if (ImGui::Begin("Load Godot Scene File",
                     &g_show_file_dialog,
                     ImGuiWindowFlags_NoCollapse))
    {
        // Current path display with copy button
        ImGui::Text("Path:");
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.9f, 1.0f, 1.0f));
        ImGui::TextWrapped(
            "%s", sanitize_utf8(g_file_dialog_current_path.string()).c_str());
        ImGui::PopStyleColor();

        ImGui::Separator();

        // Navigation buttons
        if (ImGui::Button("Up", ImVec2(60, 0)))
        {
            if (g_file_dialog_current_path.has_parent_path())
            {
                g_file_dialog_current_path =
                    g_file_dialog_current_path.parent_path();
                g_file_dialog_selected_file.clear();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Home", ImVec2(60, 0)))
        {
            g_file_dialog_current_path = std::filesystem::path(
                std::getenv("HOME") ? std::getenv("HOME") : ".");
            g_file_dialog_selected_file.clear();
        }
        ImGui::SameLine();
        if (ImGui::Button("Current", ImVec2(70, 0)))
        {
            g_file_dialog_current_path = std::filesystem::current_path();
            g_file_dialog_selected_file.clear();
        }
        ImGui::SameLine();
        if (ImGui::Button("Assets", ImVec2(70, 0)))
        {
            g_file_dialog_current_path = std::filesystem::path("assets");
            g_file_dialog_selected_file.clear();
        }

        ImGui::Separator();

        // File list
        ImGui::BeginChild("##file_list",
                          ImVec2(0, -80),
                          true,
                          ImGuiWindowFlags_HorizontalScrollbar);

        try
        {
            // Show parent directory
            if (g_file_dialog_current_path.has_parent_path())
            {
                ImGui::PushStyleColor(ImGuiCol_Text,
                                      ImVec4(0.7f, 0.7f, 1.0f, 1.0f));
                bool is_selected = false;
                if (ImGui::Selectable("[..]", is_selected, 0, ImVec2(0, 22)))
                {
                    g_file_dialog_current_path =
                        g_file_dialog_current_path.parent_path();
                    g_file_dialog_selected_file.clear();
                }
                ImGui::PopStyleColor();
            }

            // List directories and glTF/GLB files (Godot export format)
            std::vector<std::filesystem::path> entries;
            for (const auto& entry : std::filesystem::directory_iterator(
                     g_file_dialog_current_path))
            {
                entries.push_back(entry.path());
            }
            std::sort(entries.begin(),
                      entries.end(),
                      [](const auto& a, const auto& b)
                      {
                          bool a_is_dir = std::filesystem::is_directory(a);
                          bool b_is_dir = std::filesystem::is_directory(b);
                          if (a_is_dir != b_is_dir)
                              return a_is_dir > b_is_dir; // Directories first
                          return a.filename().string() < b.filename().string();
                      });

            for (const auto& entry : entries)
            {
                bool is_dir = std::filesystem::is_directory(entry);
                auto ext    = entry.extension().string();
                std::ranges::transform(ext, ext.begin(), ::tolower);
                bool is_gltf = !is_dir && (ext == ".gltf" || ext == ".glb");

                if (!is_dir && !is_gltf)
                    continue; // Skip non-glTF files

                std::string name = sanitize_utf8(entry.filename().string());
                // Compare normalized paths for selection
                bool is_selected = false;
                if (!g_file_dialog_selected_file.empty())
                {
                    try
                    {
                        auto normalized_entry =
                            std::filesystem::canonical(entry);
                        is_selected =
                            (g_file_dialog_selected_file == normalized_entry);
                    }
                    catch (...)
                    {
                        try
                        {
                            auto abs_entry = std::filesystem::absolute(entry)
                                                 .lexically_normal();
                            auto abs_selected = std::filesystem::absolute(
                                                    g_file_dialog_selected_file)
                                                    .lexically_normal();
                            is_selected = (abs_selected == abs_entry);
                        }
                        catch (...)
                        {
                            is_selected =
                                (g_file_dialog_selected_file == entry);
                        }
                    }
                }

                if (is_dir)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text,
                                          ImVec4(0.5f, 0.8f, 0.5f, 1.0f));
                    std::string dir_label = "[DIR] " + name;
                    if (ImGui::Selectable(dir_label.c_str(),
                                          false,
                                          ImGuiSelectableFlags_AllowDoubleClick,
                                          ImVec2(0, 22)))
                    {
                        if (ImGui::IsMouseDoubleClicked(0))
                        {
                            g_file_dialog_current_path = entry;
                            g_file_dialog_selected_file.clear();
                        }
                    }
                    ImGui::PopStyleColor();
                }
                else if (is_gltf)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text,
                                          ImVec4(1.0f, 0.9f, 0.5f, 1.0f));
                    if (ImGui::Selectable(name.c_str(),
                                          is_selected,
                                          ImGuiSelectableFlags_AllowDoubleClick,
                                          ImVec2(0, 22)))
                    {
                        // Always update selection on click - normalize path
                        try
                        {
                            g_file_dialog_selected_file =
                                std::filesystem::canonical(entry);
                            spdlog::debug("Selected file (canonical): {}",
                                          g_file_dialog_selected_file.string());
                        }
                        catch (const std::exception&)
                        {
                            try
                            {
                                g_file_dialog_selected_file =
                                    std::filesystem::absolute(entry)
                                        .lexically_normal();
                                spdlog::debug(
                                    "Selected file (absolute): {}",
                                    g_file_dialog_selected_file.string());
                            }
                            catch (const std::exception& e2)
                            {
                                g_file_dialog_selected_file = entry;
                                spdlog::warn("Could not normalize path, using "
                                             "raw: {}, error: {}",
                                             entry.string(),
                                             e2.what());
                            }
                        }
                        log(2,
                            "Selected file: " +
                                sanitize_utf8(
                                    g_file_dialog_selected_file.string()));

                        // Double-click to load immediately
                        if (ImGui::IsMouseDoubleClicked(0))
                        {
                            load_gltf_scene(g_file_dialog_selected_file);
                        }
                    }
                    ImGui::PopStyleColor();
                }
            }
        }
        catch (const std::exception& e)
        {
            std::string error_msg = sanitize_utf8(e.what());
            ImGui::TextColored(
                ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Error: %s", error_msg.c_str());
        }

        ImGui::EndChild();

        ImGui::Separator();

        // Selected file display and action buttons
        ImGui::Text("Selected:");
        ImGui::SameLine();
        if (!g_file_dialog_selected_file.empty())
        {
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  ImVec4(1.0f, 0.9f, 0.5f, 1.0f));
            std::string filename =
                g_file_dialog_selected_file.filename().string();
            ImGui::Text("%s", sanitize_utf8(filename).c_str());
            ImGui::PopStyleColor();
        }
        else
        {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "None");
        }

        ImGui::Spacing();

        // Action buttons
        bool has_selection = false;
        if (!g_file_dialog_selected_file.empty())
        {
            try
            {
                has_selection =
                    std::filesystem::exists(g_file_dialog_selected_file) &&
                    std::filesystem::is_regular_file(
                        g_file_dialog_selected_file);
            }
            catch (...)
            {
                has_selection = false;
            }
        }

        // Debug info
        if (!g_file_dialog_selected_file.empty() && !has_selection)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f),
                               "File not found or invalid");
        }

        ImGui::BeginDisabled(!has_selection);
        if (ImGui::Button("Open", ImVec2(100, 30)))
        {
            if (has_selection && !g_file_dialog_selected_file.empty())
            {
                load_gltf_scene(g_file_dialog_selected_file);
            }
            else
            {
                log(4, "Open button clicked but no valid selection");
            }
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 30)))
        {
            g_show_file_dialog = false;
            g_file_dialog_selected_file.clear();
        }

        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                           "Double-click to open");
    }
    ImGui::End();
}

} // namespace ui
