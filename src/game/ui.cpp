#include "ui.hpp"
#include "scene.hpp"

#include <core-api/camera.hpp>
#include <imgui.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <format>
#include <ranges>

namespace ui
{

namespace
{

// Modern Steam 2024+ inspired theme - clean, flat, professional
void apply_theme()
{
    ImGuiStyle& s = ImGui::GetStyle();
    ImVec4* c = s.Colors;

    // Modern dark palette
    const ImVec4 bg_deep   = ImVec4(0.067f, 0.067f, 0.075f, 1.0f);  // #111113
    const ImVec4 bg_main   = ImVec4(0.098f, 0.098f, 0.110f, 1.0f);  // #19191C
    const ImVec4 bg_panel  = ImVec4(0.129f, 0.129f, 0.145f, 1.0f);  // #212125
    const ImVec4 bg_hover  = ImVec4(0.180f, 0.180f, 0.200f, 1.0f);  // #2E2E33
    const ImVec4 bg_active = ImVec4(0.220f, 0.220f, 0.245f, 1.0f);  // #38383E

    // Modern accent - gradient-like blue-cyan
    const ImVec4 accent    = ImVec4(0.380f, 0.680f, 0.934f, 1.0f);  // #61ADEE
    const ImVec4 accent_h  = ImVec4(0.480f, 0.750f, 0.970f, 1.0f);  // #7AC0F8
    const ImVec4 accent_d  = ImVec4(0.280f, 0.550f, 0.800f, 1.0f);  // #478CCC

    // Text
    const ImVec4 text      = ImVec4(0.950f, 0.950f, 0.960f, 1.0f);
    const ImVec4 text_dim  = ImVec4(0.550f, 0.550f, 0.580f, 1.0f);

    c[ImGuiCol_Text]                  = text;
    c[ImGuiCol_TextDisabled]          = text_dim;
    c[ImGuiCol_WindowBg]              = bg_main;
    c[ImGuiCol_ChildBg]               = bg_deep;
    c[ImGuiCol_PopupBg]               = ImVec4(bg_panel.x, bg_panel.y, bg_panel.z, 0.98f);
    c[ImGuiCol_Border]                = ImVec4(0.20f, 0.20f, 0.22f, 0.50f);
    c[ImGuiCol_BorderShadow]          = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_FrameBg]               = bg_panel;
    c[ImGuiCol_FrameBgHovered]        = bg_hover;
    c[ImGuiCol_FrameBgActive]         = bg_active;
    c[ImGuiCol_TitleBg]               = bg_deep;
    c[ImGuiCol_TitleBgActive]         = bg_panel;
    c[ImGuiCol_TitleBgCollapsed]      = bg_deep;
    c[ImGuiCol_MenuBarBg]             = bg_deep;
    c[ImGuiCol_ScrollbarBg]           = bg_deep;
    c[ImGuiCol_ScrollbarGrab]         = bg_hover;
    c[ImGuiCol_ScrollbarGrabHovered]  = accent_d;
    c[ImGuiCol_ScrollbarGrabActive]   = accent;
    c[ImGuiCol_CheckMark]             = accent;
    c[ImGuiCol_SliderGrab]            = accent_d;
    c[ImGuiCol_SliderGrabActive]      = accent;
    c[ImGuiCol_Button]                = bg_hover;
    c[ImGuiCol_ButtonHovered]         = accent_d;
    c[ImGuiCol_ButtonActive]          = accent;
    c[ImGuiCol_Header]                = bg_hover;
    c[ImGuiCol_HeaderHovered]         = accent_d;
    c[ImGuiCol_HeaderActive]          = accent;
    c[ImGuiCol_Separator]             = ImVec4(0.25f, 0.25f, 0.28f, 1.0f);
    c[ImGuiCol_SeparatorHovered]      = accent_d;
    c[ImGuiCol_SeparatorActive]       = accent;
    c[ImGuiCol_ResizeGrip]            = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ResizeGripHovered]     = accent_d;
    c[ImGuiCol_ResizeGripActive]      = accent;
    c[ImGuiCol_Tab]                   = bg_panel;
    c[ImGuiCol_TabHovered]            = accent_d;
    c[ImGuiCol_TabActive]             = accent;
    c[ImGuiCol_TabUnfocused]          = bg_main;
    c[ImGuiCol_TabUnfocusedActive]    = bg_hover;
    c[ImGuiCol_PlotLines]             = accent;
    c[ImGuiCol_PlotLinesHovered]      = accent_h;
    c[ImGuiCol_PlotHistogram]         = accent_d;
    c[ImGuiCol_PlotHistogramHovered]  = accent;
    c[ImGuiCol_TableHeaderBg]         = bg_panel;
    c[ImGuiCol_TableBorderStrong]     = bg_active;
    c[ImGuiCol_TableBorderLight]      = bg_hover;
    c[ImGuiCol_TableRowBg]            = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_TableRowBgAlt]         = ImVec4(1, 1, 1, 0.015f);
    c[ImGuiCol_TextSelectedBg]        = ImVec4(accent.x, accent.y, accent.z, 0.30f);
    c[ImGuiCol_DragDropTarget]        = accent_h;
    c[ImGuiCol_NavHighlight]          = accent;
    c[ImGuiCol_NavWindowingHighlight] = text;
    c[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.05f, 0.05f, 0.06f, 0.20f);
    c[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.03f, 0.03f, 0.04f, 0.75f);

    // Modern flat metrics
    s.WindowRounding    = 8.0f;
    s.ChildRounding     = 4.0f;
    s.FrameRounding     = 4.0f;
    s.PopupRounding     = 8.0f;
    s.ScrollbarRounding = 8.0f;
    s.GrabRounding      = 4.0f;
    s.TabRounding       = 6.0f;
    
    s.WindowPadding     = ImVec2(12, 12);
    s.FramePadding      = ImVec2(10, 6);
    s.ItemSpacing       = ImVec2(8, 6);
    s.ItemInnerSpacing  = ImVec2(6, 6);
    s.IndentSpacing     = 20.0f;
    s.ScrollbarSize     = 14.0f;
    s.GrabMinSize       = 12.0f;
    
    s.WindowBorderSize  = 0.0f;
    s.ChildBorderSize   = 0.0f;
    s.PopupBorderSize   = 1.0f;
    s.FrameBorderSize   = 0.0f;
    s.TabBorderSize     = 0.0f;
    
    s.WindowTitleAlign  = ImVec2(0.5f, 0.5f);
    s.WindowMenuButtonPosition = ImGuiDir_None;
    s.ColorButtonPosition = ImGuiDir_Right;
    s.ButtonTextAlign   = ImVec2(0.5f, 0.5f);
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
            if (ImGui::MenuItem("Rescan Assets"))
            {
                scene::scan_models();
                scene::scan_audio();
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
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Render"))
        {
            ImGui::MenuItem("Wireframe Mode", "Tab", &g_wireframe);
            ImGui::MenuItem("Auto Animate", "Space", &g_auto_rotate);
            ImGui::Separator();
            if (ImGui::ColorEdit3("Background", g_sky_color, 
                ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel))
                scene::apply_sky();
            ImGui::SameLine();
            ImGui::Text("Background Color");
            ImGui::EndMenu();
        }

        // Right: Stats
        auto info = std::format("{:.0f} FPS  |  {}x{}  |  {} objects",
                                ctx->time.fps,
                                ctx->settings->get_window_width(),
                                ctx->settings->get_window_height(),
                                scene::g_models.size());
        float w = ImGui::CalcTextSize(info.c_str()).x;
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - w - 20);
        ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.58f, 1.0f), "%s", info.c_str());

        ImGui::EndMainMenuBar();
    }
    ImGui::PopStyleVar();
}

void draw_scene(euengine::engine_context* ctx)
{
    if (!g_show_hierarchy) return;

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(16, 40), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(320, 600), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(280, 300), ImVec2(500, io.DisplaySize.y - 60));

    if (ImGui::Begin("Scene", &g_show_hierarchy))
    {
        // Camera section
        if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (scene::g_camera != entt::null && ctx->registry->valid(scene::g_camera))
            {
                auto& cam = ctx->registry->get<euengine::camera_component>(scene::g_camera);
                
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.55f, 0.58f, 1.0f));
                ImGui::Text("Position");
                ImGui::PopStyleColor();
                ImGui::DragFloat3("##pos", &cam.position.x, 0.2f);
                
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.55f, 0.58f, 1.0f));
                ImGui::Text("Settings");
                ImGui::PopStyleColor();
                ImGui::SliderFloat("Speed", &cam.move_speed, 1.0f, 50.0f);
                ImGui::SliderFloat("FOV", &cam.fov, 30.0f, 120.0f);
            }
        }

        // Environment section
        if (ImGui::CollapsingHeader("Environment", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.55f, 0.58f, 1.0f));
            ImGui::Text("Colors");
            ImGui::PopStyleColor();
            
            if (ImGui::ColorEdit3("Sky", g_sky_color))
                scene::apply_sky();
            if (ImGui::ColorEdit3("Grid", g_grid_color))
                scene::rebuild_grid();
        }

        // Objects list
        if (ImGui::CollapsingHeader("Objects", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.55f, 0.58f, 1.0f));
            ImGui::Text("%s", std::format("{} items", scene::g_models.size()).c_str());
            ImGui::PopStyleColor();
            
            // Filter/search for objects
            static char obj_filter[128] = {};
            ImGui::SetNextItemWidth(-1);
            ImGui::InputTextWithHint("##obj_filter", "Search objects...", obj_filter, sizeof(obj_filter));
            
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
                ImVec4 col = ImVec4(0.55f, 0.55f, 0.58f, 1.0f);
                const char* icon = "";
                if (m.moving)
                {
                    col = ImVec4(0.40f, 0.80f, 0.50f, 1.0f);
                    icon = "> ";
                }
                else if (m.hover)
                {
                    col = ImVec4(0.40f, 0.80f, 0.95f, 1.0f);
                    icon = "^ ";
                }
                else if (m.animate)
                {
                    col = ImVec4(0.95f, 0.75f, 0.30f, 1.0f);
                    icon = "~ ";
                }

                if (sel)
                {
                    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.28f, 0.55f, 0.80f, 0.6f));
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
                }
                else
                {
                    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
                    ImGui::PushStyleColor(ImGuiCol_Text, col);
                }

                auto label = std::format("{}{}", icon, m.name);
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
                    auto& cam = ctx->registry->get<euengine::camera_component>(scene::g_camera);
                    cam.position = m.transform.position + glm::vec3(0, 2, 5);
                    cam.pitch = -15.0f;
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
    // Position below Scene window (Scene is at y=40, height=600, so start at 650)
    ImGui::SetNextWindowPos(ImVec2(16, 650), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(320, 200), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(280, 150), ImVec2(500, io.DisplaySize.y - 60));

    if (ImGui::Begin("Inspector", &g_show_inspector))
    {
        if (scene::g_selected >= 0 && 
            static_cast<std::size_t>(scene::g_selected) < scene::g_models.size())
        {
            auto& m = scene::g_models[static_cast<std::size_t>(scene::g_selected)];

            // Header
            ImGui::TextColored(ImVec4(0.38f, 0.68f, 0.93f, 1.0f), "%s", m.name.c_str());
            ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.58f, 1.0f), "%s", 
                               std::filesystem::path(m.path).filename().string().c_str());
            ImGui::Separator();

            // Transform
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.55f, 0.58f, 1.0f));
            ImGui::Text("Transform");
            ImGui::PopStyleColor();
            
            ImGui::DragFloat3("Position", &m.transform.position.x, 0.05f);
            ImGui::DragFloat3("Rotation", &m.transform.rotation.x, 0.5f);
            
            float sc = m.transform.scale.x;
            if (ImGui::SliderFloat("Scale", &sc, 0.01f, 0.5f))
                m.transform.scale = glm::vec3(sc);

            ImGui::Separator();

            // Animation
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.55f, 0.58f, 1.0f));
            ImGui::Text("Animation");
            ImGui::PopStyleColor();
            
            ImGui::Checkbox("Rotate", &m.animate);
            if (m.animate)
            {
                ImGui::SameLine();
                ImGui::SetNextItemWidth(100);
                ImGui::SliderFloat("##speed", &m.anim_speed, 0.0f, 100.0f, "%.0f°/s");
            }
            ImGui::Checkbox("Hover", &m.hover);

            ImGui::Spacing();
            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.70f, 0.25f, 0.25f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.35f, 0.35f, 1.0f));
            if (ImGui::Button("Delete Object", ImVec2(-1, 32)))
                scene::remove_model(scene::g_selected);
            ImGui::PopStyleColor(2);
        }
        else
        {
            ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.58f, 1.0f), "Select an object to inspect");
        }
    }
    ImGui::End();
}

void draw_browser()
{
    if (!g_show_browser) return;

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 300, 40), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(284, 360), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(250, 200), ImVec2(450, io.DisplaySize.y - 60));

    if (ImGui::Begin("Asset Browser", &g_show_browser))
    {
        if (ImGui::Button("Refresh", ImVec2(80, 0)))
            scene::scan_models();
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.58f, 1.0f), "%s", 
                           std::format("{} models", scene::g_model_files.size()).c_str());

        ImGui::Separator();

        ImGui::BeginChild("##list", ImVec2(0, -40), true);
        for (std::size_t i = 0; i < scene::g_model_files.size(); ++i)
        {
            auto name = std::filesystem::path(scene::g_model_files[i]).filename().string();
            bool sel = std::cmp_equal(i, scene::g_browser_sel);
            if (ImGui::Selectable(name.c_str(), sel))
                scene::g_browser_sel = static_cast<int>(i);
        }
        ImGui::EndChild();

        bool ok = scene::g_browser_sel >= 0 &&
                  static_cast<std::size_t>(scene::g_browser_sel) < scene::g_model_files.size();
        
        ImGui::PushStyleColor(ImGuiCol_Button, ok ? ImVec4(0.28f, 0.55f, 0.80f, 1.0f) : ImVec4(0.2f, 0.2f, 0.22f, 1.0f));
        if (!ok) ImGui::BeginDisabled();
        if (ImGui::Button("Add to Scene", ImVec2(-1, 32)))
        {
            auto* m = scene::add_model(scene::g_model_files[static_cast<std::size_t>(scene::g_browser_sel)],
                             { 0, 0, 6 }, 0.1f);
            if (m) m->animate = true;
            scene::g_selected = static_cast<int>(scene::g_models.size()) - 1;
        }
        if (!ok) ImGui::EndDisabled();
        ImGui::PopStyleColor();
    }
    ImGui::End();
}

void draw_audio(euengine::engine_context* ctx)
{
    if (!g_show_audio || !ctx->audio) return;

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 300, 420), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(284, 240), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(250, 150), ImVec2(450, io.DisplaySize.y - 60));

    if (ImGui::Begin("Audio Player", &g_show_audio))
    {
        // Track list
        ImGui::BeginChild("##tracks", ImVec2(0, -50), true);
        for (std::size_t i = 0; i < scene::g_audio.size(); ++i)
        {
            auto& t = scene::g_audio[i];
            bool playing = std::cmp_equal(i, scene::g_playing);

            ImVec4 col = ImVec4(0.85f, 0.85f, 0.88f, 1.0f);
            if (playing)
                col = ImVec4(0.40f, 0.90f, 0.55f, 1.0f);
            else if (t.is_sfx)
                col = ImVec4(0.40f, 0.75f, 0.95f, 1.0f);

            ImGui::PushStyleColor(ImGuiCol_Text, col);
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
            ImGui::PopStyleColor();
        }
        ImGui::EndChild();

        // Controls
        bool playing = ctx->audio->is_music_playing();
        bool paused = ctx->audio->is_music_paused();

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 6));
        
        float bw = 60.0f;
        if (playing && !paused)
        {
            if (ImGui::Button("Pause", ImVec2(bw, 28)))
                ctx->audio->pause_music();
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.28f, 0.55f, 0.80f, 1.0f));
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
    if (!g_show_engine) return;

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 300, 40), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(284, 400), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(250, 250), ImVec2(450, io.DisplaySize.y - 60));

    if (ImGui::Begin("Engine Settings", &g_show_engine))
    {
        // Renderer
        ImGui::TextColored(ImVec4(0.38f, 0.68f, 0.93f, 1.0f), "Renderer");
        ImGui::Separator();
        
        ImGui::Text("%s", std::format("GPU: {}", ctx->settings->get_gpu_driver()).c_str());
        ImGui::Text("%s", std::format("Resolution: {} x {}",
                                       ctx->settings->get_window_width(),
                                       ctx->settings->get_window_height()).c_str());

        bool fs = ctx->settings->is_fullscreen();
        if (ImGui::Checkbox("Fullscreen (F11)", &fs))
            ctx->settings->set_fullscreen(fs);

        // VSync
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.55f, 0.58f, 1.0f));
        ImGui::Text("VSync Mode");
        ImGui::PopStyleColor();
        
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
            auto name = std::filesystem::path(scene::g_lib_path).filename().string();
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
                auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                    ftime - std::filesystem::file_time_type::clock::now() +
                    std::chrono::system_clock::now());
                auto time_t = std::chrono::system_clock::to_time_t(sctp);
                char buf[64];
                std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&time_t));
                ImGui::Text("Modified: %s", buf);
            }
            
            ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.58f, 1.0f), "Path: %s", 
                               scene::g_lib_path.c_str());
            
            ImGui::Spacing();
            
            // Hot reload button
            if (ImGui::Button("Hot Reload Module (F5)", ImVec2(-1, 32)))
            {
                log(2, "Game module hot reload triggered");
                if (ctx->shaders)
                {
                    ctx->shaders->enable_hot_reload(false);
                    ctx->shaders->enable_hot_reload(true);
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
    if (!g_show_stats) return;

    ImGuiIO& io = ImGui::GetIO();
    
    // Enhanced floating overlay - bottom right, larger
    float w = 380.0f;
    float h = 280.0f;
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - w - 16, io.DisplaySize.y - h - 40));
    ImGui::SetNextWindowSize(ImVec2(w, h));
    ImGui::SetNextWindowBgAlpha(0.92f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 10));
    
    if (ImGui::Begin("##perf", nullptr, flags))
    {
        // Header with close button
        ImGui::TextColored(ImVec4(0.38f, 0.68f, 0.93f, 1.0f), "Performance Metrics");
        ImGui::SameLine(ImGui::GetWindowWidth() - 28);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        if (ImGui::SmallButton("x"))
            g_show_stats = false;
        ImGui::PopStyleColor();
        
        ImGui::Separator();
        
        // Current stats
        auto perf_text = std::format("{:.2f} ms  |  {:.0f} FPS", 
                                      ctx->time.delta * 1000.0f, ctx->time.fps);
        ImGui::Text("%s", perf_text.c_str());
        
        // FPS stats
        if (scene::g_max_fps > 0.0f)
        {
            auto fps_stats = std::format("FPS: Min {:.0f} | Avg {:.0f} | Max {:.0f}",
                                          scene::g_min_fps, scene::g_avg_fps, scene::g_max_fps);
            ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.58f, 1.0f), "%s", fps_stats.c_str());
        }
        
        auto frame_text = std::format("Frame {}  |  {:.1f}s elapsed",
                                       ctx->time.frame_count, ctx->time.elapsed);
        ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.58f, 1.0f), "%s", frame_text.c_str());
        
        // Render stats
        auto render_text = std::format("Draw Calls: {}  |  Triangles: ~{}",
                                        scene::g_draw_calls, scene::g_triangles);
        ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.58f, 1.0f), "%s", render_text.c_str());
        
        ImGui::Spacing();
        
        // Performance graphs using ImGui PlotLines
        constexpr int size = 300;
        int idx = scene::g_frame_idx;
        float reordered_times[300];
        float reordered_fps[300];
        
        // Reorder circular buffers for plotting
        for (int i = 0; i < size; ++i)
        {
            int src_idx = (idx + i) % size;
            if (src_idx >= 0 && src_idx < size)
            {
                reordered_times[i] = scene::g_frame_times[src_idx];
                reordered_fps[i] = scene::g_fps_history[src_idx];
            }
            else
            {
                reordered_times[i] = 0.0f;
                reordered_fps[i] = 0.0f;
            }
        }
        
        // Frame time graph with legend
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.55f, 0.58f, 1.0f));
        ImGui::Text("Frame Time (ms)");
        ImGui::PopStyleColor();
        ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(0.38f, 0.68f, 0.93f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.067f, 0.067f, 0.075f, 1.0f));
        ImGui::PlotLines("##frame_time", reordered_times, size, 0, nullptr, 0.0f, 33.3f, ImVec2(-1, 60));
        ImGui::PopStyleColor(2);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.45f, 0.48f, 1.0f));
        ImGui::Text("  0 ms                                16.67 ms (60 FPS)");
        ImGui::PopStyleColor();
        
        ImGui::Spacing();
        
        // FPS history graph with legend
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.55f, 0.58f, 1.0f));
        ImGui::Text("FPS History");
        ImGui::PopStyleColor();
        ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(0.40f, 0.80f, 0.50f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.067f, 0.067f, 0.075f, 1.0f));
        ImGui::PlotLines("##fps_history", reordered_fps, size, 0, nullptr, 0.0f, 120.0f, ImVec2(-1, 60));
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
    if (!g_show_console) return;

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(16, io.DisplaySize.y - 280), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(600, 240), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(400, 120), ImVec2(io.DisplaySize.x - 32, 500));

    if (ImGui::Begin("Console", &g_show_console))
    {
        // Toolbar
        if (ImGui::Button("Clear", ImVec2(60, 0)))
            log_clear();
        ImGui::SameLine();
        
        // Filter input
        ImGui::SetNextItemWidth(200);
        char filter_buf[256] = {};
        std::strncpy(filter_buf, g_console_filter.c_str(), sizeof(filter_buf) - 1);
        if (ImGui::InputTextWithHint("##filter", "Filter...", filter_buf, sizeof(filter_buf)))
            g_console_filter = filter_buf;
        ImGui::SameLine();
        
        std::size_t entry_count = g_console_filter.empty() 
            ? g_log.size() 
            : static_cast<std::size_t>(std::ranges::count_if(g_log, [](const auto& e) {
                return e.message.find(g_console_filter) != std::string::npos;
            }));
        ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.58f, 1.0f), "%s", 
                           std::format("{} entries", entry_count).c_str());

        ImGui::Separator();

        // Log view
        ImGui::BeginChild("##log", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
        
        bool should_scroll = false;
        for (const auto& e : g_log)
        {
            // Apply filter
            if (!g_console_filter.empty() && 
                e.message.find(g_console_filter) == std::string::npos)
                continue;
            
            // Timestamp
            auto time_str = std::format("[{:.1f}]", e.time);
            ImGui::TextColored(ImVec4(0.45f, 0.45f, 0.48f, 1.0f), "%s", time_str.c_str());
            ImGui::SameLine();
            
            // Level tag and color
            ImVec4 col;
            const char* tag;
            switch (e.level)
            {
                case 0: col = ImVec4(0.50f, 0.50f, 0.53f, 1.0f); tag = "TRACE"; break;
                case 1: col = ImVec4(0.60f, 0.60f, 0.65f, 1.0f); tag = "DEBUG"; break;
                case 2: col = ImVec4(0.38f, 0.68f, 0.93f, 1.0f); tag = "INFO"; break;
                case 3: col = ImVec4(0.95f, 0.75f, 0.30f, 1.0f); tag = "WARN"; break;
                case 4: col = ImVec4(0.95f, 0.40f, 0.40f, 1.0f); tag = "ERROR"; break;
                default: col = ImVec4(0.70f, 0.70f, 0.70f, 1.0f); tag = "???"; break;
            }

            ImGui::TextColored(col, "[%s]", tag);
            ImGui::SameLine();
            ImGui::TextWrapped("%s", e.message.c_str());
            
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

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoSavedSettings;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 6));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.067f, 0.067f, 0.075f, 1.0f));

    if (ImGui::Begin("##bar", nullptr, flags))
    {
        const char* help = ctx->input.mouse_captured
            ? "WASD Move | QE Up/Down | Shift Speed | Space Animate | Tab Wireframe | F5 Reload | ESC Release"
            : "Click to capture mouse | F5 Reload | F11 Fullscreen | ` Console";
        
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
