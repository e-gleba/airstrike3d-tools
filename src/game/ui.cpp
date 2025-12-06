#include "ui.hpp"
#include "scene.hpp"

#include <core-api/camera.hpp>
#include <imgui.h>

#include <algorithm>
#include <cstring>
#include <filesystem>

namespace ui
{

namespace
{

// Steam/Valve-inspired minimalist dark theme
void apply_theme()
{
    ImGuiStyle& s = ImGui::GetStyle();
    ImVec4* c = s.Colors;

    // Base colors - dark, minimal, professional
    const ImVec4 bg0 = ImVec4(0.10f, 0.10f, 0.11f, 1.0f);  // Darkest
    const ImVec4 bg1 = ImVec4(0.14f, 0.14f, 0.15f, 1.0f);  // Window
    const ImVec4 bg2 = ImVec4(0.18f, 0.18f, 0.19f, 1.0f);  // Child/Frame
    const ImVec4 bg3 = ImVec4(0.22f, 0.22f, 0.24f, 1.0f);  // Hover
    const ImVec4 bg4 = ImVec4(0.28f, 0.28f, 0.30f, 1.0f);  // Active

    // Accent - subtle steam blue-green
    const ImVec4 acc0 = ImVec4(0.40f, 0.56f, 0.56f, 1.0f);
    const ImVec4 acc1 = ImVec4(0.50f, 0.70f, 0.70f, 1.0f);
    const ImVec4 acc2 = ImVec4(0.60f, 0.82f, 0.82f, 1.0f);

    // Text
    const ImVec4 txt0 = ImVec4(0.92f, 0.92f, 0.94f, 1.0f);
    const ImVec4 txt1 = ImVec4(0.60f, 0.60f, 0.64f, 1.0f);

    c[ImGuiCol_Text]                  = txt0;
    c[ImGuiCol_TextDisabled]          = txt1;
    c[ImGuiCol_WindowBg]              = bg1;
    c[ImGuiCol_ChildBg]               = bg0;
    c[ImGuiCol_PopupBg]               = ImVec4(0.12f, 0.12f, 0.13f, 0.98f);
    c[ImGuiCol_Border]                = ImVec4(0.25f, 0.25f, 0.28f, 0.60f);
    c[ImGuiCol_BorderShadow]          = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_FrameBg]               = bg2;
    c[ImGuiCol_FrameBgHovered]        = bg3;
    c[ImGuiCol_FrameBgActive]         = bg4;
    c[ImGuiCol_TitleBg]               = bg0;
    c[ImGuiCol_TitleBgActive]         = ImVec4(0.12f, 0.12f, 0.13f, 1.0f);
    c[ImGuiCol_TitleBgCollapsed]      = bg0;
    c[ImGuiCol_MenuBarBg]             = bg0;
    c[ImGuiCol_ScrollbarBg]           = bg0;
    c[ImGuiCol_ScrollbarGrab]         = bg3;
    c[ImGuiCol_ScrollbarGrabHovered]  = acc0;
    c[ImGuiCol_ScrollbarGrabActive]   = acc1;
    c[ImGuiCol_CheckMark]             = acc2;
    c[ImGuiCol_SliderGrab]            = acc0;
    c[ImGuiCol_SliderGrabActive]      = acc1;
    c[ImGuiCol_Button]                = bg3;
    c[ImGuiCol_ButtonHovered]         = acc0;
    c[ImGuiCol_ButtonActive]          = acc1;
    c[ImGuiCol_Header]                = bg3;
    c[ImGuiCol_HeaderHovered]         = ImVec4(acc0.x, acc0.y, acc0.z, 0.7f);
    c[ImGuiCol_HeaderActive]          = acc0;
    c[ImGuiCol_Separator]             = ImVec4(0.30f, 0.30f, 0.32f, 1.0f);
    c[ImGuiCol_SeparatorHovered]      = acc0;
    c[ImGuiCol_SeparatorActive]       = acc1;
    c[ImGuiCol_ResizeGrip]            = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ResizeGripHovered]     = acc0;
    c[ImGuiCol_ResizeGripActive]      = acc1;
    c[ImGuiCol_Tab]                   = bg2;
    c[ImGuiCol_TabHovered]            = acc0;
    c[ImGuiCol_TabActive]             = ImVec4(acc0.x * 0.8f, acc0.y * 0.8f, acc0.z * 0.8f, 1.0f);
    c[ImGuiCol_TabUnfocused]          = bg1;
    c[ImGuiCol_TabUnfocusedActive]    = bg3;
    c[ImGuiCol_PlotLines]             = acc1;
    c[ImGuiCol_PlotLinesHovered]      = acc2;
    c[ImGuiCol_PlotHistogram]         = acc0;
    c[ImGuiCol_PlotHistogramHovered]  = acc1;
    c[ImGuiCol_TableHeaderBg]         = bg2;
    c[ImGuiCol_TableBorderStrong]     = bg4;
    c[ImGuiCol_TableBorderLight]      = bg3;
    c[ImGuiCol_TableRowBg]            = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_TableRowBgAlt]         = ImVec4(1, 1, 1, 0.02f);
    c[ImGuiCol_TextSelectedBg]        = ImVec4(acc0.x, acc0.y, acc0.z, 0.35f);
    c[ImGuiCol_DragDropTarget]        = acc2;
    c[ImGuiCol_NavHighlight]          = acc1;
    c[ImGuiCol_NavWindowingHighlight] = txt0;
    c[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.1f, 0.1f, 0.1f, 0.2f);
    c[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.05f, 0.05f, 0.06f, 0.70f);

    // Tight, modern metrics
    s.WindowRounding    = 4.0f;
    s.ChildRounding     = 2.0f;
    s.FrameRounding     = 2.0f;
    s.PopupRounding     = 4.0f;
    s.ScrollbarRounding = 6.0f;
    s.GrabRounding      = 2.0f;
    s.TabRounding       = 2.0f;
    
    s.WindowPadding     = ImVec2(8, 8);
    s.FramePadding      = ImVec2(6, 4);
    s.ItemSpacing       = ImVec2(6, 4);
    s.ItemInnerSpacing  = ImVec2(4, 4);
    s.IndentSpacing     = 16.0f;
    s.ScrollbarSize     = 12.0f;
    s.GrabMinSize       = 8.0f;
    
    s.WindowBorderSize  = 1.0f;
    s.ChildBorderSize   = 0.0f;
    s.PopupBorderSize   = 1.0f;
    s.FrameBorderSize   = 0.0f;
    s.TabBorderSize     = 0.0f;
    
    s.WindowTitleAlign  = ImVec2(0.0f, 0.5f);
    s.WindowMenuButtonPosition = ImGuiDir_None;
    s.ColorButtonPosition = ImGuiDir_Right;
    s.ButtonTextAlign   = ImVec2(0.5f, 0.5f);
    s.SelectableTextAlign = ImVec2(0.0f, 0.0f);
}

void draw_menu(euengine::engine_context* ctx)
{
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Hot Reload", "F5"))
            {
                log(2, "Hot reload");
                if (ctx->shaders)
                {
                    ctx->shaders->enable_hot_reload(false);
                    ctx->shaders->enable_hot_reload(true);
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Scan Models")) scene::scan_models();
            if (ImGui::MenuItem("Scan Audio")) scene::scan_audio();
            ImGui::Separator();
            if (ImGui::MenuItem("Quit", "Alt+F4")) ctx->settings->request_quit();
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View"))
        {
            ImGui::MenuItem("Scene", nullptr, &g_show_hierarchy);
            ImGui::MenuItem("Inspector", nullptr, &g_show_inspector);
            ImGui::MenuItem("Browser", nullptr, &g_show_browser);
            ImGui::MenuItem("Audio", nullptr, &g_show_audio);
            ImGui::Separator();
            ImGui::MenuItem("Engine", nullptr, &g_show_engine);
            ImGui::MenuItem("Stats", nullptr, &g_show_stats);
            ImGui::MenuItem("Console", "`", &g_show_console);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Scene"))
        {
            ImGui::MenuItem("Wireframe", "Tab", &g_wireframe);
            ImGui::MenuItem("Auto Rotate", "Space", &g_auto_rotate);
            ImGui::EndMenu();
        }

        // Right-aligned info
        char info[128];
        snprintf(info, sizeof(info), "%.0f FPS  |  %d x %d  |  %zu objects",
                 ctx->time.fps,
                 ctx->settings->get_window_width(),
                 ctx->settings->get_window_height(),
                 scene::g_models.size());
        float w = ImGui::CalcTextSize(info).x;
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - w - 16);
        ImGui::TextDisabled("%s", info);

        ImGui::EndMainMenuBar();
    }
}

void draw_scene(euengine::engine_context* ctx)
{
    if (!g_show_hierarchy) return;

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(8, 28), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(200, 340), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(150, 100), ImVec2(350, io.DisplaySize.y - 60));

    if (ImGui::Begin("Scene", &g_show_hierarchy))
    {
        // Camera
        if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (scene::g_camera != entt::null && ctx->registry->valid(scene::g_camera))
            {
                auto& cam = ctx->registry->get<euengine::camera_component>(scene::g_camera);
                ImGui::DragFloat3("Position", &cam.position.x, 0.2f);
                ImGui::SliderFloat("Speed", &cam.move_speed, 1.0f, 40.0f);
                ImGui::SliderFloat("FOV", &cam.fov, 30.0f, 110.0f);
            }
        }

        // Environment
        if (ImGui::CollapsingHeader("Environment"))
        {
            if (ImGui::ColorEdit3("Sky", g_sky_color, ImGuiColorEditFlags_NoInputs))
                scene::apply_sky();
            if (ImGui::ColorEdit3("Grid", g_grid_color, ImGuiColorEditFlags_NoInputs))
                scene::rebuild_grid();
        }

        // Objects
        if (ImGui::CollapsingHeader("Objects", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::BeginChild("##objs", ImVec2(0, 0), false);
            for (std::size_t i = 0; i < scene::g_models.size(); ++i)
            {
                auto& m = scene::g_models[i];
                bool sel = (static_cast<int>(i) == scene::g_selected);

                ImGuiTreeNodeFlags f = ImGuiTreeNodeFlags_Leaf |
                                       ImGuiTreeNodeFlags_NoTreePushOnOpen |
                                       ImGuiTreeNodeFlags_SpanAvailWidth;
                if (sel) f |= ImGuiTreeNodeFlags_Selected;

                // Color by type
                if (m.hover)
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.8f, 0.8f, 1.0f));
                else if (m.animate)
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.7f, 0.5f, 1.0f));

                ImGui::TreeNodeEx(reinterpret_cast<void*>(i), f, "%s", m.name.c_str());

                if (m.hover || m.animate)
                    ImGui::PopStyleColor();

                if (ImGui::IsItemClicked())
                    scene::g_selected = static_cast<int>(i);

                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0) &&
                    scene::g_camera != entt::null)
                {
                    auto& cam = ctx->registry->get<euengine::camera_component>(scene::g_camera);
                    cam.position = m.transform.position + glm::vec3(0, 2, 6);
                    cam.pitch = -12.0f;
                    cam.yaw = -90.0f;
                }
            }
            ImGui::EndChild();
        }
    }
    ImGui::End();
}

void draw_inspector()
{
    if (!g_show_inspector) return;

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(8, 380), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(200, 200), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(150, 80), ImVec2(350, io.DisplaySize.y - 60));

    if (ImGui::Begin("Inspector", &g_show_inspector))
    {
        if (scene::g_selected >= 0 && 
            static_cast<std::size_t>(scene::g_selected) < scene::g_models.size())
        {
            auto& m = scene::g_models[static_cast<std::size_t>(scene::g_selected)];

            ImGui::TextColored(ImVec4(0.6f, 0.85f, 0.85f, 1.0f), "%s", m.name.c_str());
            ImGui::Separator();

            ImGui::DragFloat3("Position", &m.transform.position.x, 0.1f);
            ImGui::DragFloat3("Rotation", &m.transform.rotation.x, 1.0f);
            
            float sc = m.transform.scale.x;
            if (ImGui::SliderFloat("Scale", &sc, 0.01f, 0.5f))
                m.transform.scale = glm::vec3(sc);

            ImGui::Separator();
            ImGui::Checkbox("Animate", &m.animate);
            if (m.animate)
                ImGui::SliderFloat("Speed", &m.anim_speed, 0.0f, 80.0f);
            ImGui::Checkbox("Hover", &m.hover);

            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.2f, 0.2f, 0.8f));
            if (ImGui::Button("Delete", ImVec2(-1, 0)))
                scene::remove_model(scene::g_selected);
            ImGui::PopStyleColor();
        }
        else
        {
            ImGui::TextDisabled("No selection");
        }
    }
    ImGui::End();
}

void draw_browser()
{
    if (!g_show_browser) return;

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 240, 28), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(232, 280), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Browser", &g_show_browser))
    {
        if (ImGui::Button("Scan")) scene::scan_models();
        ImGui::SameLine();
        ImGui::TextDisabled("%zu", scene::g_model_files.size());

        ImGui::Separator();

        ImGui::BeginChild("##list", ImVec2(0, -30), true);
        for (std::size_t i = 0; i < scene::g_model_files.size(); ++i)
        {
            auto name = std::filesystem::path(scene::g_model_files[i]).filename().string();
            if (ImGui::Selectable(name.c_str(), std::cmp_equal(i, scene::g_browser_sel)))
                scene::g_browser_sel = static_cast<int>(i);
        }
        ImGui::EndChild();

        bool ok = scene::g_browser_sel >= 0 &&
                  static_cast<std::size_t>(scene::g_browser_sel) < scene::g_model_files.size();
        if (!ok) ImGui::BeginDisabled();
        if (ImGui::Button("Load", ImVec2(-1, 0)))
        {
            scene::add_model(scene::g_model_files[static_cast<std::size_t>(scene::g_browser_sel)],
                             { 0, 0, 8 }, 0.1f);
            scene::g_selected = static_cast<int>(scene::g_models.size()) - 1;
        }
        if (!ok) ImGui::EndDisabled();
    }
    ImGui::End();
}

void draw_audio(euengine::engine_context* ctx)
{
    if (!g_show_audio || !ctx->audio) return;

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 240, 320), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(232, 220), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Audio", &g_show_audio))
    {
        // Track list
        ImGui::BeginChild("##tracks", ImVec2(0, -70), true);
        for (std::size_t i = 0; i < scene::g_audio.size(); ++i)
        {
            auto& t = scene::g_audio[i];
            bool playing = std::cmp_equal(i, scene::g_playing);

            if (playing)
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.9f, 0.6f, 1.0f));
            else if (t.is_sfx)
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.7f, 0.8f, 1.0f));

            if (ImGui::Selectable(t.name.c_str(), playing))
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

            if (playing || t.is_sfx)
                ImGui::PopStyleColor();
        }
        ImGui::EndChild();

        // Timeline placeholder
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.15f, 0.15f, 0.16f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.5f, 0.7f, 0.7f, 1.0f));
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderFloat("##pos", &g_music_pos, 0.0f, 100.0f, "%.0f%%");
        ImGui::PopStyleColor(2);

        // Controls
        bool playing = ctx->audio->is_music_playing();
        bool paused = ctx->audio->is_music_paused();

        if (playing && !paused)
        {
            if (ImGui::Button("Pause", ImVec2(50, 0))) ctx->audio->pause_music();
        }
        else
        {
            if (ImGui::Button("Play", ImVec2(50, 0))) ctx->audio->resume_music();
        }
        ImGui::SameLine();
        if (ImGui::Button("Stop", ImVec2(50, 0)))
        {
            ctx->audio->stop_music();
            scene::g_playing = -1;
        }
        ImGui::SameLine();
        
        // Volume (0-100)
        ImGui::SetNextItemWidth(-1);
        if (ImGui::SliderFloat("##vol", &g_volume, 0.0f, 100.0f, "%.0f%%"))
            ctx->audio->set_music_volume(g_volume / 100.0f);
    }
    ImGui::End();
}

void draw_engine(euengine::engine_context* ctx)
{
    if (!g_show_engine) return;

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 240, 28), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(232, 280), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Engine", &g_show_engine))
    {
        // Display
        ImGui::TextColored(ImVec4(0.6f, 0.85f, 0.85f, 1.0f), "Display");
        ImGui::Text("GPU: %s", ctx->settings->get_gpu_driver().data());
        ImGui::Text("Resolution: %d x %d",
                    ctx->settings->get_window_width(),
                    ctx->settings->get_window_height());

        bool fs = ctx->settings->is_fullscreen();
        if (ImGui::Checkbox("Fullscreen", &fs))
            ctx->settings->set_fullscreen(fs);

        // VSync with all options
        int vs = static_cast<int>(ctx->settings->get_vsync());
        ImGui::Text("VSync:");
        ImGui::SameLine();
        if (ImGui::RadioButton("Off", vs == 0))
            ctx->settings->set_vsync(euengine::vsync_mode::disabled);
        ImGui::SameLine();
        if (ImGui::RadioButton("On", vs == 1))
            ctx->settings->set_vsync(euengine::vsync_mode::enabled);
        ImGui::SameLine();
        if (ImGui::RadioButton("Adaptive", vs == 2))
            ctx->settings->set_vsync(euengine::vsync_mode::adaptive);

        ImGui::Separator();

        // Shaders
        ImGui::TextColored(ImVec4(0.6f, 0.85f, 0.85f, 1.0f), "Shaders");
        if (ctx->shaders)
        {
            bool hot = ctx->shaders->hot_reload_enabled();
            if (ImGui::Checkbox("Hot Reload", &hot))
                ctx->shaders->enable_hot_reload(hot);
            ImGui::SameLine();
            if (ImGui::Button("Reload Now"))
            {
                log(2, "Hot reload");
                ctx->shaders->enable_hot_reload(false);
                ctx->shaders->enable_hot_reload(true);
            }
        }

        ImGui::Separator();

        // Module
        ImGui::TextColored(ImVec4(0.6f, 0.85f, 0.85f, 1.0f), "Game Module");
        if (!scene::g_lib_path.empty())
        {
            auto name = std::filesystem::path(scene::g_lib_path).filename().string();
            ImGui::Text("%s", name.c_str());
            ImGui::Text("%.1f KB", static_cast<float>(scene::g_lib_size) / 1024.0f);
        }
        else
        {
            ImGui::TextDisabled("Built-in");
        }
    }
    ImGui::End();
}

void draw_stats(euengine::engine_context* ctx)
{
    if (!g_show_stats) return;

    ImGuiIO& io = ImGui::GetIO();
    
    // Bottom-right overlay
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 180, io.DisplaySize.y - 120),
                            ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(170, 90), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.7f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoFocusOnAppearing;

    if (ImGui::Begin("##stats", nullptr, flags))
    {
        ImGui::TextColored(ImVec4(0.6f, 0.85f, 0.85f, 1.0f), "Performance");
        ImGui::Text("%.1f ms  (%.0f FPS)", ctx->time.delta * 1000.0f, ctx->time.fps);
        ImGui::Text("Frame: %lu", static_cast<unsigned long>(ctx->time.frame_count));
        
        // Frame time graph
        ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(0.5f, 0.8f, 0.7f, 1.0f));
        ImGui::PlotLines("##ft", scene::g_frame_times, 120, scene::g_frame_idx, 
                         nullptr, 0.0f, 33.3f, ImVec2(-1, 30));
        ImGui::PopStyleColor();
    }
    ImGui::End();
}

void draw_console()
{
    if (!g_show_console) return;

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(8, io.DisplaySize.y - 200), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(450, 170), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(250, 80), ImVec2(io.DisplaySize.x - 16, 300));

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.08f, 0.09f, 0.95f));
    
    if (ImGui::Begin("Console", &g_show_console))
    {
        if (ImGui::Button("Clear")) log_clear();
        ImGui::SameLine();
        ImGui::TextDisabled("%zu entries", g_log.size());

        ImGui::Separator();

        ImGui::BeginChild("##log", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
        
        for (const auto& e : g_log)
        {
            ImVec4 col;
            const char* tag;
            switch (e.level)
            {
                case 0: col = ImVec4(0.5f, 0.5f, 0.5f, 1.0f); tag = "[trace]"; break;
                case 1: col = ImVec4(0.6f, 0.6f, 0.7f, 1.0f); tag = "[debug]"; break;
                case 2: col = ImVec4(0.5f, 0.8f, 0.8f, 1.0f); tag = "[info]"; break;
                case 3: col = ImVec4(0.9f, 0.7f, 0.3f, 1.0f); tag = "[warn]"; break;
                case 4: col = ImVec4(0.95f, 0.4f, 0.4f, 1.0f); tag = "[error]"; break;
                default: col = ImVec4(0.7f, 0.7f, 0.7f, 1.0f); tag = "[?]"; break;
            }

            ImGui::TextDisabled("[%.1f]", e.time);
            ImGui::SameLine();
            ImGui::TextColored(col, "%s", tag);
            ImGui::SameLine();
            ImGui::TextWrapped("%s", e.message.c_str());
        }

        if (g_log_scroll)
        {
            ImGui::SetScrollHereY(1.0f);
            g_log_scroll = false;
        }

        ImGui::EndChild();
    }
    ImGui::End();
    ImGui::PopStyleColor();
}

void draw_statusbar(euengine::engine_context* ctx)
{
    ImGuiIO& io = ImGui::GetIO();

    ImGui::SetNextWindowPos(ImVec2(0, io.DisplaySize.y - 22));
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, 22));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoSavedSettings;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 3));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.08f, 0.09f, 1.0f));

    if (ImGui::Begin("##bar", nullptr, flags))
    {
        const char* help = ctx->input.mouse_captured
            ? "WASD Move | QE Up/Down | Shift Fast | Space Rotate | Tab Wire | F5 Reload | ESC Release"
            : "Click viewport to capture | F5 Reload | F11 Fullscreen | ` Console";
        ImGui::TextDisabled("%s", help);
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
    draw_menu(ctx);
    draw_scene(ctx);
    draw_inspector();
    draw_browser();
    draw_audio(ctx);
    draw_engine(ctx);
    draw_stats(ctx);
    draw_console();
    draw_statusbar(ctx);
}

} // namespace ui
