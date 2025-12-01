#include "camera.hpp"
#include "game_api.hpp"
#include "scripting.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>
#include <spdlog/spdlog.h>

#include <cmath>
#include <limits>
#include <vector>

namespace
{

as3::engine_context*          g_ctx = nullptr;
std::vector<as3::mesh_handle> g_meshes;

// Scripting
as3::ScriptManager                g_scripts;
std::vector<as3::compound_object> g_objects;
std::size_t                       g_selected_object = SIZE_MAX;

// Camera
entt::entity g_camera_entity = entt::null;

// State
float g_time      = 0.0f;
bool  g_wireframe = false;

// UI state
bool g_show_settings   = false;
bool g_show_script_log = false;
bool g_show_inspector  = true;

// Mouse
bool g_mouse_was_pressed_left  = false;
bool g_mouse_was_pressed_right = false;

// Convert screen position to world position on ground plane
glm::vec3 screen_to_ground(float                       screen_x,
                           float                       screen_y,
                           const as3::CameraComponent& cam,
                           float                       screen_w,
                           float                       screen_h)
{
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

    float ndc_x = (2.0f * screen_x / screen_w) - 1.0f;
    float ndc_y = 1.0f - (2.0f * screen_y / screen_h);

    glm::mat4 inv_proj = glm::inverse(proj);
    glm::mat4 inv_view = glm::inverse(view);

    glm::vec4 ray_clip(ndc_x, ndc_y, -1.0f, 1.0f);
    glm::vec4 ray_eye = inv_proj * ray_clip;
    ray_eye           = glm::vec4(ray_eye.x, ray_eye.y, -1.0f, 0.0f);

    glm::vec3 ray_world = glm::normalize(glm::vec3(inv_view * ray_eye));

    if (std::abs(ray_world.y) < 0.0001f)
        return glm::vec3(0.0f);

    float t = -cam.position.y / ray_world.y;
    if (t < 0.0f)
        return glm::vec3(0.0f);

    return cam.position + ray_world * t;
}

// Try to select object at world position
void try_select_at(const glm::vec3& pos)
{
    // Deselect current
    if (g_selected_object < g_objects.size())
    {
        g_scripts.on_deselect(g_objects[g_selected_object]);
        g_objects[g_selected_object].selected = false;
    }
    g_selected_object = SIZE_MAX;

    // Find closest object within selection radius
    float       best_dist = 999999.0f;
    std::size_t best_idx  = SIZE_MAX;

    for (std::size_t i = 0; i < g_objects.size(); ++i)
    {
        auto& obj  = g_objects[i];
        float dx   = pos.x - obj.position.x;
        float dz   = pos.z - obj.position.z;
        float dist = std::sqrt(dx * dx + dz * dz);

        if (dist < obj.selection_radius && dist < best_dist)
        {
            best_dist = dist;
            best_idx  = i;
        }
    }

    if (best_idx != SIZE_MAX)
    {
        g_selected_object            = best_idx;
        g_objects[best_idx].selected = true;
        g_scripts.on_select(g_objects[best_idx]);
    }
}

// Command selected object to move
void command_move(const glm::vec3& target)
{
    if (g_selected_object < g_objects.size())
    {
        auto& obj = g_objects[g_selected_object];
        if (obj.can_move)
        {
            g_scripts.on_move_command(obj, target);
        }
    }
}

// Update object movement (simple ground movement)
void update_object_movement(as3::compound_object& obj, float dt)
{
    if (!obj.has_target || !obj.can_move)
        return;

    float dx   = obj.target_pos.x - obj.position.x;
    float dz   = obj.target_pos.z - obj.position.z;
    float dist = std::sqrt(dx * dx + dz * dz);

    if (dist < 0.5f)
    {
        obj.has_target    = false;
        obj.current_speed = 0.0f;
        return;
    }

    // Calculate target angle
    float target_angle = glm::degrees(std::atan2(dx, dz));
    float angle_diff   = target_angle - obj.rotation.y;
    while (angle_diff > 180.0f)
        angle_diff -= 360.0f;
    while (angle_diff < -180.0f)
        angle_diff += 360.0f;

    // Rotate toward target
    float max_rot = obj.turn_speed * dt;
    if (std::abs(angle_diff) > max_rot)
    {
        obj.rotation.y += (angle_diff > 0 ? max_rot : -max_rot);
    }
    else
    {
        obj.rotation.y = target_angle;
    }

    // Move forward if roughly facing target
    if (std::abs(angle_diff) < 30.0f)
    {
        obj.current_speed =
            std::min(obj.current_speed + obj.move_speed * dt, obj.move_speed);
        float move_dist = obj.current_speed * dt;
        obj.position.x += std::sin(glm::radians(obj.rotation.y)) * move_dist;
        obj.position.z += std::cos(glm::radians(obj.rotation.y)) * move_dist;
    }
}

// Render compound object with all parts
void render_object(as3::IRenderer* renderer, as3::compound_object& obj)
{
    for (auto& part : obj.parts)
    {
        if (part.model == as3::invalid_model)
            continue;

        // Build transform: object transform + part hierarchy
        as3::transform xform;
        xform.position = obj.position + part.offset;
        xform.rotation = obj.rotation + part.rotation;
        xform.scale    = obj.scale * part.scale;

        // TODO: proper hierarchy transform (parent chain)

        renderer->draw_model(part.model, xform);
    }
}

} // namespace

GAME_API bool game_init(as3::engine_context* ctx)
{
    g_ctx  = ctx;
    g_time = 0.0f;

    // Initialize scripting
    g_scripts.init(ctx->renderer);

    // Setup camera
    if (g_camera_entity != entt::null &&
        g_ctx->registry->valid(g_camera_entity))
        g_ctx->registry->destroy(g_camera_entity);

    g_camera_entity = g_ctx->registry->create();
    auto& cam = g_ctx->registry->emplace<as3::CameraComponent>(g_camera_entity);
    cam.position   = { 0.0f, 25.0f, 40.0f };
    cam.pitch      = -30.0f;
    cam.yaw        = -90.0f;
    cam.move_speed = 20.0f;

    // Ground grid
    g_meshes.push_back(ctx->renderer->create_wireframe_grid(
        100.0f, 100, { 0.15f, 0.2f, 0.15f }));

    // Load demo objects
    g_objects.clear();

    // T-72 Tank
    as3::compound_object tank;
    tank.position = { -10.0f, 0.0f, 0.0f };
    if (g_scripts.load_object(tank, "assets/scripts/objects/t72.lua"))
    {
        g_objects.push_back(std::move(tank));
    }

    // Kamov helicopter
    as3::compound_object heli;
    heli.position = { 10.0f, 5.0f, 0.0f };
    if (g_scripts.load_object(heli, "assets/scripts/objects/kamov.lua"))
    {
        g_objects.push_back(std::move(heli));
    }

    // Duck (glTF sample)
    as3::compound_object duck;
    duck.position = { 0.0f, 0.0f, 15.0f };
    if (g_scripts.load_object(duck, "assets/scripts/objects/duck.lua"))
    {
        g_objects.push_back(std::move(duck));
    }

    ctx->renderer->set_render_mode(as3::render_mode::textured);

    spdlog::info("Game initialized with {} objects", g_objects.size());
    return true;
}

GAME_API void game_shutdown()
{
    // Unload object models
    for (auto& obj : g_objects)
    {
        for (auto& part : obj.parts)
        {
            if (part.model != as3::invalid_model && g_ctx->renderer)
            {
                g_ctx->renderer->unload_model(part.model);
            }
        }
    }
    g_objects.clear();

    g_scripts.shutdown();

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

    // Handle mouse input
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

            if (left_pressed && !g_mouse_was_pressed_left)
            {
                glm::vec3 world_pos = screen_to_ground(io.MousePos.x,
                                                       io.MousePos.y,
                                                       cam,
                                                       io.DisplaySize.x,
                                                       io.DisplaySize.y);
                try_select_at(world_pos);
            }

            if (right_pressed && !g_mouse_was_pressed_right)
            {
                glm::vec3 world_pos = screen_to_ground(io.MousePos.x,
                                                       io.MousePos.y,
                                                       cam,
                                                       io.DisplaySize.x,
                                                       io.DisplaySize.y);
                command_move(world_pos);
            }

            g_mouse_was_pressed_left  = left_pressed;
            g_mouse_was_pressed_right = right_pressed;
        }
    }

    // Update all objects
    for (auto& obj : g_objects)
    {
        update_object_movement(obj, ctx->delta_time);
        g_scripts.update_object(obj, ctx->delta_time);
    }
}

GAME_API void game_render(as3::engine_context* ctx)
{
    // Ground grid
    for (auto h : g_meshes)
        ctx->renderer->draw(h);

    // Render all objects
    for (auto& obj : g_objects)
    {
        render_object(ctx->renderer, obj);

        // Draw bounding box for selected objects
        if (obj.selected)
        {
            // Calculate combined bounds of all parts
            as3::bounds combined{};
            combined.min = glm::vec3(std::numeric_limits<float>::max());
            combined.max = glm::vec3(std::numeric_limits<float>::lowest());

            for (const auto& part : obj.parts)
            {
                if (part.model != as3::invalid_model)
                {
                    const auto part_bounds =
                        ctx->renderer->get_bounds(part.model);
                    // Transform part bounds by part offset
                    combined.min =
                        glm::min(combined.min, part_bounds.min + part.offset);
                    combined.max =
                        glm::max(combined.max, part_bounds.max + part.offset);
                }
            }

            // Expand bounds slightly for visibility
            constexpr float k_bounds_padding = 0.2f;
            combined.min -= glm::vec3(k_bounds_padding);
            combined.max += glm::vec3(k_bounds_padding);

            // Draw the bounding box
            as3::transform box_xform;
            box_xform.position = obj.position;
            box_xform.rotation = obj.rotation;
            box_xform.scale    = obj.scale;

            ctx->renderer->draw_bounds(
                combined, box_xform, { 0.0f, 1.0f, 0.0f });
        }
    }
}

GAME_API void game_ui(as3::engine_context* ctx)
{
    ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx->imgui_ctx));
    ImGuiIO& io = ImGui::GetIO();

    // ===== MAIN MENU BAR =====
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Reload Scripts", "F5"))
            {
                for (auto& obj : g_objects)
                    (void)g_scripts.reload_object(obj);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Quit"))
                ctx->settings->request_quit();
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View"))
        {
            ImGui::MenuItem("Inspector", nullptr, &g_show_inspector);
            ImGui::MenuItem("Script Log", nullptr, &g_show_script_log);
            ImGui::MenuItem("Engine Settings", nullptr, &g_show_settings);
            ImGui::Separator();
            ImGui::MenuItem("Wireframe", nullptr, &g_wireframe);
            ImGui::EndMenu();
        }

        // Right side - FPS
        float fps = 1.0f / ctx->delta_time;
        char  fps_text[32];
        snprintf(fps_text, sizeof(fps_text), "%.0f FPS", fps);
        float text_width = ImGui::CalcTextSize(fps_text).x;
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - text_width - 10);
        ImGui::TextColored(ImVec4(0.5f, 0.8f, 0.5f, 1.0f), "%s", fps_text);

        ImGui::EndMainMenuBar();
    }

    // ===== INSPECTOR PANEL (Right side) =====
    if (g_show_inspector)
    {
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 300, 25),
                                ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(290, 500), ImGuiCond_FirstUseEver);

        if (ImGui::Begin("Inspector", &g_show_inspector))
        {
            // Scene stats
            ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "Scene");
            ImGui::Separator();
            ImGui::Text("Objects: %zu", g_objects.size());

            const auto stats = ctx->renderer->get_stats();
            ImGui::Text("Draw calls: %u", stats.draw_calls);
            ImGui::Text("Triangles: %u", stats.triangles);

            ImGui::Spacing();

            // Objects list
            ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "Objects");
            ImGui::Separator();

            for (std::size_t i = 0; i < g_objects.size(); ++i)
            {
                auto& obj      = g_objects[i];
                bool  selected = (i == g_selected_object);

                if (ImGui::Selectable(obj.name.c_str(), selected))
                {
                    if (g_selected_object < g_objects.size())
                    {
                        g_objects[g_selected_object].selected = false;
                        g_scripts.on_deselect(g_objects[g_selected_object]);
                    }
                    g_selected_object = i;
                    obj.selected      = true;
                    g_scripts.on_select(obj);
                }
            }

            ImGui::Spacing();

            // Selected object properties
            if (g_selected_object < g_objects.size())
            {
                auto& obj = g_objects[g_selected_object];

                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f),
                                   "Selected: %s",
                                   obj.name.c_str());
                ImGui::Separator();

                // Transform
                ImGui::DragFloat3("Position", &obj.position.x, 0.1f);
                ImGui::DragFloat3("Rotation", &obj.rotation.x, 1.0f);
                ImGui::DragFloat("Scale", &obj.scale.x, 0.01f, 0.1f, 10.0f);
                obj.scale.y = obj.scale.z = obj.scale.x;

                ImGui::Spacing();

                // Movement
                ImGui::Text("Speed: %.1f", obj.current_speed);
                ImGui::Checkbox("Can Move", &obj.can_move);
                ImGui::DragFloat(
                    "Move Speed", &obj.move_speed, 0.5f, 0.0f, 100.0f);
                ImGui::DragFloat(
                    "Turn Speed", &obj.turn_speed, 1.0f, 0.0f, 360.0f);

                ImGui::Spacing();

                // Parts
                ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f),
                                   "Parts (%zu)",
                                   obj.parts.size());
                ImGui::Separator();

                for (std::size_t pi = 0; pi < obj.parts.size(); ++pi)
                {
                    auto& part = obj.parts[pi];
                    ImGui::PushID(static_cast<int>(pi));

                    if (ImGui::TreeNode(part.name.c_str()))
                    {
                        // Transform editing
                        ImGui::DragFloat3("Offset", &part.offset.x, 0.05f);
                        ImGui::DragFloat3(
                            "Scale", &part.scale.x, 0.01f, 0.1f, 5.0f);

                        if (part.can_rotate)
                        {
                            ImGui::Separator();
                            ImGui::Text("Angle: %.1f", part.current_angle);
                            if (!part.continuous)
                            {
                                ImGui::SliderFloat("Target",
                                                   &part.target_angle,
                                                   part.min_angle,
                                                   part.max_angle);
                            }
                            ImGui::Checkbox("Continuous", &part.continuous);
                            ImGui::DragFloat("Speed",
                                             &part.rotation_speed,
                                             1.0f,
                                             0.0f,
                                             1000.0f);
                        }
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                }

                ImGui::Spacing();
                ImGui::Separator();

                // Export button - prints current offsets as Lua code
                if (ImGui::Button("Copy Offsets to Log"))
                {
                    spdlog::info("-- {} part offsets:", obj.name);
                    for (const auto& part : obj.parts)
                    {
                        spdlog::info(
                            "{}.offset = vec3.new({:.2f}, {:.2f}, {:.2f})",
                            part.name,
                            part.offset.x,
                            part.offset.y,
                            part.offset.z);
                        spdlog::info(
                            "{}.scale = vec3.new({:.2f}, {:.2f}, {:.2f})",
                            part.name,
                            part.scale.x,
                            part.scale.y,
                            part.scale.z);
                    }
                }
                ImGui::SameLine();

                // Reload button
                if (ImGui::Button("Reload Script"))
                {
                    (void)g_scripts.reload_object(obj);
                }
            }
        }
        ImGui::End();
    }

    // ===== SCRIPT LOG =====
    g_scripts.draw_log_window(&g_show_script_log);

    // ===== ENGINE SETTINGS =====
    if (g_show_settings && ctx->settings)
    {
        ImGui::SetNextWindowSize(ImVec2(300, 350), ImGuiCond_FirstUseEver);

        if (ImGui::Begin("Engine Settings", &g_show_settings))
        {
            ImGui::Text("Resolution: %dx%d",
                        ctx->settings->get_window_width(),
                        ctx->settings->get_window_height());
            ImGui::Text("GPU: %s", ctx->settings->get_gpu_driver().data());

            bool fullscreen = ctx->settings->is_fullscreen();
            if (ImGui::Checkbox("Fullscreen (F11)", &fullscreen))
                ctx->settings->set_fullscreen(fullscreen);

            ImGui::Spacing();
            ImGui::Text("VSync:");
            int vsync = static_cast<int>(ctx->settings->get_vsync());
            if (ImGui::RadioButton("Off", &vsync, 0))
                ctx->settings->set_vsync(as3::vsync_mode::disabled);
            ImGui::SameLine();
            if (ImGui::RadioButton("On", &vsync, 1))
                ctx->settings->set_vsync(as3::vsync_mode::enabled);
            ImGui::SameLine();
            if (ImGui::RadioButton("Adaptive", &vsync, 2))
                ctx->settings->set_vsync(as3::vsync_mode::adaptive);

            if (ctx->audio)
            {
                ImGui::Spacing();
                ImGui::Separator();
                float music = ctx->audio->get_music_volume();
                if (ImGui::SliderFloat("Music", &music, 0.0f, 1.0f))
                    ctx->audio->set_music_volume(music);
                float sound = ctx->audio->get_sound_volume();
                if (ImGui::SliderFloat("Sound", &sound, 0.0f, 1.0f))
                    ctx->audio->set_sound_volume(sound);
            }

            if (ctx->shaders)
            {
                ImGui::Spacing();
                ImGui::Separator();
                bool hot = ctx->shaders->hot_reload_enabled();
                if (ImGui::Checkbox("Shader Hot Reload", &hot))
                    ctx->shaders->enable_hot_reload(hot);
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
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                           "WASD/QE - fly | Click - select | Right-click - "
                           "move | F5 - reload | F11 - fullscreen");

        // Script error indicator
        if (g_scripts.has_errors())
        {
            ImGui::SameLine(ImGui::GetWindowWidth() - 100);
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f),
                               "ERRORS: %d",
                               g_scripts.error_count());
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
}
