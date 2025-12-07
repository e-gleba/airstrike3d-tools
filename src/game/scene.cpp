#include "scene.hpp"
#include "ui.hpp"

#include <core-api/camera.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <ranges>

namespace scene
{

namespace
{

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
constexpr int key_grave  = 53;

void setup_scene()
{
    rebuild_grid();

    // Organized showcase layout - spread out for better visibility
    // All animations disabled by default

    // Front row: vehicles - moved further back and to sides to not block duck
    add_model("assets/models/tanks/t72/t72_base.obj", { -15, 0, 15 }, 0.07f);
    add_model(
        "assets/models/tanks/sherman/sherman_base.obj", { -8, 0, 15 }, 0.07f);
    add_model(
        "assets/models/btrs/btr_rocket/btr_rocket.obj", { 8, 0, 15 }, 0.07f);
    add_model("assets/models/cannons/aagunvulcan/aagunvulcan_base.obj",
              { 15, 0, 15 },
              0.09f);

    // Moving jeep - rides in a wider loop (behind tanks, away from duck)
    if (auto* m =
            add_model("assets/models/jeeps/uaz/uaz.obj", { -18, 0, 10 }, 0.07f))
    {
        m->moving               = true;
        m->move_speed           = 0.4f;
        m->move_dir             = 1.0f;
        m->move_start           = { -18, 0, 10 };
        m->move_end             = { 18, 0, 10 };
        m->transform.rotation.y = 90.0f;
    }

    // Middle row: aircraft - moved further back, higher up, wider spacing
    if (auto* m = add_model(
            "assets/models/helics/kamov/kamov.obj", { -10, 1.6f, 0 }, 0.07f))
    {
        m->hover       = true;
        m->hover_base  = 1.6f;
        m->hover_range = 0.12f;
    }
    if (auto* m = add_model(
            "assets/models/helics/mi_24/mi_24.obj", { 0, 1.9f, 0 }, 0.07f))
    {
        m->hover       = true;
        m->hover_base  = 1.9f;
        m->hover_speed = 1.2f;
        m->hover_range = 0.14f;
    }
    if (auto* m = add_model(
            "assets/models/helics/cobra/cobra.obj", { 10, 1.7f, 0 }, 0.07f))
    {
        m->hover       = true;
        m->hover_base  = 1.7f;
        m->hover_speed = 1.5f;
        m->hover_range = 0.10f;
    }

    // Back row: structures - much wider spacing to avoid overlap
    add_model("assets/models/mapobjects/cisterns/cisterna01.obj",
              { -26, 0, -16 },
              0.09f);
    if (auto* temple = add_model("assets/models/mapobjects/houses/temple.obj",
                                 { -9, 0, -16 },
                                 0.08f))
    {
        temple->transform.rotation.y = 180.0f; // Rotate 180 degrees
    }
    add_model("assets/models/mapobjects/factory/oil_refinery/oil_refinery.obj",
              { 9, 0, -16 },
              0.07f);
    add_model(
        "assets/models/mapobjects/radar/radar.obj", { 26, 0, -16 }, 0.09f);

    // Sides: ships - further out and spread
    add_model("assets/models/ships/lodka/lodka.obj", { -20, 0, -4 }, 0.06f);
    add_model("assets/models/ships/rocket_boat/rocket_boat.obj",
              { 20, 0, -4 },
              0.05f);

    // Center: featured duck - very close to camera, static, smaller
    if (auto* m = add_model(
            "assets/models/samples/duck.glb", { 0, 0.1f, 22 }, 0.025f))
    {
        m->transform.rotation.y =
            180.0f; // Face camera initially, but static (no rotation)
    }
}

void process_input()
{
    if (g_camera == entt::null || !g_ctx->registry->valid(g_camera))
        return;

    auto& cam = g_ctx->registry->get<euengine::camera_component>(g_camera);

    // Allow escape to release mouse even when not captured
    if (g_ctx->input.keyboard && g_ctx->input.keyboard[key_escape] &&
        g_ctx->input.mouse_captured)
    {
        g_ctx->settings->set_mouse_captured(false);
    }

    // Only process camera input when mouse is captured (camera focused)
    if (!g_ctx->input.mouse_captured)
        return;

    // Mouse look
    cam.yaw += g_ctx->input.mouse_xrel * cam.look_speed;
    cam.pitch -= g_ctx->input.mouse_yrel * cam.look_speed;
    cam.pitch = glm::clamp(cam.pitch, -89.0f, 89.0f);

    if (g_ctx->input.keyboard == nullptr)
        return;

    float speed = cam.move_speed * g_ctx->time.delta;
    if (g_ctx->input.keyboard[key_lshift])
        speed *= 3.0f;

    glm::vec3 front = cam.front();
    glm::vec3 right = cam.right();

    if (g_ctx->input.keyboard[key_w])
        cam.position += front * speed;
    if (g_ctx->input.keyboard[key_s])
        cam.position -= front * speed;
    if (g_ctx->input.keyboard[key_a])
        cam.position -= right * speed;
    if (g_ctx->input.keyboard[key_d])
        cam.position += right * speed;
    if (g_ctx->input.keyboard[key_e])
        cam.position.y += speed;
    if (g_ctx->input.keyboard[key_q])
        cam.position.y -= speed;

    static bool keys[8] = {};

    if (g_ctx->input.keyboard[key_space] && !keys[0])
        ui::g_auto_rotate = !ui::g_auto_rotate;
    keys[0] = g_ctx->input.keyboard[key_space];

    if (g_ctx->input.keyboard[key_tab] && !keys[1])
        ui::g_wireframe = !ui::g_wireframe;
    keys[1] = g_ctx->input.keyboard[key_tab];

    if (g_ctx->input.keyboard[key_f11] && !keys[2])
        g_ctx->settings->set_fullscreen(!g_ctx->settings->is_fullscreen());
    keys[2] = g_ctx->input.keyboard[key_f11];

    if (g_ctx->input.keyboard[key_f5] && !keys[3])
    {
        ui::log(2, "Hot reload (F5)");
        if (g_ctx->shaders)
        {
            g_ctx->shaders->enable_hot_reload(false);
            g_ctx->shaders->enable_hot_reload(true);
        }
    }
    keys[3] = g_ctx->input.keyboard[key_f5];

    if (g_ctx->input.keyboard[key_grave] && !keys[4])
        ui::g_show_console = !ui::g_show_console;
    keys[4] = g_ctx->input.keyboard[key_grave];

    g_ctx->renderer->set_view_projection(cam.projection(g_ctx->display.aspect) *
                                         cam.view());
}

void animate(float t, float dt)
{
    for (auto& m : g_models)
    {
        // Animate rotation (duck is static, won't animate)
        if (m.animate && ui::g_auto_rotate)
        {
            m.transform.rotation.y += m.anim_speed * dt;
            if (m.transform.rotation.y > 360.0f)
                m.transform.rotation.y -= 360.0f;
        }
        if (m.hover)
        {
            m.transform.position.y =
                m.hover_base + std::sin(t * m.hover_speed) * m.hover_range;
        }
        if (m.moving && ui::g_auto_rotate)
        {
            // Move along path (ping-pong)
            m.move_path += m.move_speed * dt * m.move_dir;

            if (m.move_path >= 1.0f)
            {
                m.move_path            = 2.0f - m.move_path;
                m.move_dir             = -1.0f;
                m.transform.rotation.y = 270.0f; // Face left
            }
            else if (m.move_path <= 0.0f)
            {
                m.move_path            = -m.move_path;
                m.move_dir             = 1.0f;
                m.transform.rotation.y = 90.0f; // Face right
            }

            // Interpolate position
            m.transform.position =
                glm::mix(m.move_start, m.move_end, m.move_path);
        }
    }
}

} // namespace

void init(euengine::engine_context* ctx)
{
    g_ctx = ctx;

    // Get lib info
    std::filesystem::path exe = std::filesystem::current_path();
    std::filesystem::path lib = exe / "libgame.so";
    if (!std::filesystem::exists(lib))
        lib = exe / "game.dll";

    if (std::filesystem::exists(lib))
    {
        g_lib_path = lib.string();
        g_lib_size = std::filesystem::file_size(lib);
    }

    // Camera
    if (g_camera != entt::null && ctx->registry->valid(g_camera))
        ctx->registry->destroy(g_camera);

    g_camera     = ctx->registry->create();
    auto& cam    = ctx->registry->emplace<euengine::camera_component>(g_camera);
    cam.position = { 0, 4, 16 };
    cam.pitch    = -8;
    cam.yaw      = -90;
    cam.move_speed = 12;
    cam.look_speed = 0.10f;
    cam.fov        = 60;
    cam.far_plane  = 200;

    setup_scene();
    scan_models();
    scan_audio();
    scan_scenes();

    ctx->renderer->set_render_mode(euengine::render_mode::textured);
    apply_sky();

    ui::log(2,
            "Scene initialized: " + std::to_string(g_models.size()) +
                " objects");
}

void shutdown()
{
    for (auto& m : g_models)
        if (m.handle != euengine::invalid_model && g_ctx->renderer)
            g_ctx->renderer->unload_model(m.handle);
    g_models.clear();

    for (auto& a : g_audio)
        if (a.handle != euengine::invalid_music && g_ctx->audio)
            g_ctx->audio->unload_music(a.handle);
    g_audio.clear();

    for (auto h : g_grids)
        if (h != euengine::invalid_mesh && g_ctx->renderer)
            g_ctx->renderer->destroy_mesh(h);
    g_grids.clear();

    if (g_origin_axis != euengine::invalid_mesh && g_ctx->renderer)
        g_ctx->renderer->destroy_mesh(g_origin_axis);
    g_origin_axis = euengine::invalid_mesh;

    if (g_camera != entt::null && g_ctx->registry &&
        g_ctx->registry->valid(g_camera))
    {
        g_ctx->registry->destroy(g_camera);
        g_camera = entt::null;
    }

    g_ctx = nullptr;
}

void update(euengine::engine_context* ctx)
{
    // Stats
    constexpr int history_size = 300;
    g_frame_times[g_frame_idx] = ctx->time.delta * 1000.0f;
    g_fps_history[g_frame_idx] = ctx->time.fps;
    g_frame_idx                = (g_frame_idx + 1) % history_size;

    // Calculate FPS stats
    g_min_fps     = 999.0f;
    g_max_fps     = 0.0f;
    float fps_sum = 0.0f;
    for (int i = 0; i < history_size; ++i)
    {
        if (g_fps_history[i] > 0.0f)
        {
            g_min_fps = std::min(g_min_fps, g_fps_history[i]);
            g_max_fps = std::max(g_max_fps, g_fps_history[i]);
            fps_sum += g_fps_history[i];
        }
    }
    g_avg_fps = fps_sum / history_size;

    g_draw_calls = static_cast<int>(g_models.size() + g_grids.size());
    g_triangles  = static_cast<int>(g_models.size()) * 500; // estimate

    ui::g_time = ctx->time.elapsed;

    ctx->renderer->set_render_mode(ui::g_wireframe
                                       ? euengine::render_mode::wireframe
                                       : euengine::render_mode::textured);

    process_input();
    animate(ctx->time.elapsed, ctx->time.delta);
}

void render(euengine::engine_context* ctx)
{
    // Draw grid first
    for (auto h : g_grids)
        ctx->renderer->draw(h);

    // Draw origin axis gizmo after grid so it appears on top
    if (g_show_origin && g_origin_axis != euengine::invalid_mesh)
        ctx->renderer->draw(g_origin_axis);

    // Draw models last
    for (auto& m : g_models)
    {
        ctx->renderer->draw_model(m.handle, m.transform);
        if (&m - g_models.data() == g_selected)
            ctx->renderer->draw_bounds(
                m.bounds, m.transform, { 1.0f, 0.6f, 0.1f });
    }
}

void scan_models()
{
    g_model_files.clear();
    g_browser_sel = -1;

    const std::string dir = "assets/models";
    if (!std::filesystem::exists(dir))
        return;

    for (const auto& e : std::filesystem::recursive_directory_iterator(dir))
    {
        if (!e.is_regular_file())
            continue;
        auto ext = e.path().extension().string();
        if (ext == ".obj" || ext == ".glb" || ext == ".gltf" || ext == ".OBJ" ||
            ext == ".GLB" || ext == ".GLTF")
            g_model_files.push_back(e.path().string());
    }
    std::sort(g_model_files.begin(), g_model_files.end());
    ui::log(2, "Models: " + std::to_string(g_model_files.size()));
}

void scan_scenes()
{
    g_scene_files.clear();

    const std::string dir = "assets";
    if (!std::filesystem::exists(dir))
        return;

    for (const auto& e : std::filesystem::recursive_directory_iterator(dir))
    {
        if (!e.is_regular_file())
            continue;
        auto ext = e.path().extension().string();
        std::ranges::transform(ext, ext.begin(), ::tolower);
        if (ext == ".gltf" || ext == ".glb")
            g_scene_files.push_back(e.path().string());
    }
    std::sort(g_scene_files.begin(), g_scene_files.end());
    ui::log(2, "Scene files: " + std::to_string(g_scene_files.size()));
}

void scan_audio()
{
    g_audio.clear();

    auto scan_dir = [](const std::string& dir, bool sfx)
    {
        if (!std::filesystem::exists(dir))
            return;
        for (const auto& e : std::filesystem::directory_iterator(dir))
        {
            if (!e.is_regular_file())
                continue;
            auto ext = e.path().extension().string();
            if (ext == ".ogg" || ext == ".mp3" || ext == ".wav" ||
                ext == ".OGG" || ext == ".MP3" || ext == ".WAV")
            {
                audio_file f;
                f.name   = e.path().filename().string();
                f.path   = e.path().string();
                f.is_sfx = sfx;
                g_audio.push_back(f);
            }
        }
    };

    scan_dir("assets/music", false);
    scan_dir("assets/sounds", true);

    std::ranges::sort(g_audio,
                      [](const auto& a, const auto& b)
                      {
                          if (a.is_sfx != b.is_sfx)
                              return !a.is_sfx;
                          return a.name < b.name;
                      });

    ui::log(2, "Audio: " + std::to_string(g_audio.size()) + " tracks");
}

model_instance* add_model(const std::string& path,
                          const glm::vec3&   pos,
                          float              scale)
{
    auto handle = g_ctx->renderer->load_model(path);
    if (handle == euengine::invalid_model)
    {
        ui::log(4,
                "Failed to load: " +
                    std::filesystem::path(path).filename().string());
        return nullptr;
    }

    model_instance m;
    m.handle             = handle;
    m.path               = path;
    m.name               = std::filesystem::path(path).stem().string();
    m.bounds             = g_ctx->renderer->get_bounds(handle);
    m.transform.position = pos;
    m.transform.scale    = glm::vec3(scale);
    m.hover_base         = pos.y;

    std::string name = m.name; // capture before move
    g_models.push_back(std::move(m));
    ui::log(2, "Loaded: " + name);
    return &g_models.back();
}

void remove_model(int idx)
{
    if (idx < 0 || static_cast<std::size_t>(idx) >= g_models.size())
        return;
    g_ctx->renderer->unload_model(
        g_models[static_cast<std::size_t>(idx)].handle);
    g_models.erase(g_models.begin() + idx);
    g_selected = -1;
}

model_instance* duplicate_model(int idx)
{
    if (idx < 0 || static_cast<std::size_t>(idx) >= g_models.size())
        return nullptr;

    const auto& src = g_models[static_cast<std::size_t>(idx)];

    // Create duplicate with offset position
    glm::vec3 new_pos = src.transform.position + glm::vec3(2.0f, 0.0f, 2.0f);

    model_instance m;
    m.handle             = g_ctx->renderer->load_model(src.path);
    m.path               = src.path;
    m.name               = src.name + "_copy";
    m.bounds             = g_ctx->renderer->get_bounds(m.handle);
    m.transform          = src.transform;
    m.transform.position = new_pos;
    m.animate            = src.animate;
    m.anim_speed         = src.anim_speed;
    m.hover              = src.hover;
    m.hover_base         = new_pos.y;
    m.hover_speed        = src.hover_speed;
    m.hover_range        = src.hover_range;
    m.moving             = src.moving;
    m.move_speed         = src.move_speed;
    m.move_start         = src.move_start + glm::vec3(2.0f, 0.0f, 2.0f);
    m.move_end           = src.move_end + glm::vec3(2.0f, 0.0f, 2.0f);
    m.color_tint         = src.color_tint;

    g_models.push_back(std::move(m));
    g_selected = static_cast<int>(g_models.size()) - 1;
    ui::log(2, "Duplicated: " + src.name);
    return &g_models.back();
}

void focus_camera_on_object(int idx)
{
    if (idx < 0 || static_cast<std::size_t>(idx) >= g_models.size())
        return;
    if (g_camera == entt::null || !g_ctx->registry->valid(g_camera))
        return;

    const auto& obj = g_models[static_cast<std::size_t>(idx)];
    auto& cam = g_ctx->registry->get<euengine::camera_component>(g_camera);

    // Get object position and size
    glm::vec3 obj_pos  = obj.transform.position;
    glm::vec3 obj_size = obj.bounds.max - obj.bounds.min;
    float     max_dim  = std::max({ obj_size.x, obj_size.y, obj_size.z });

    // Calculate viewing distance - closer to object
    float view_dist = std::max(max_dim * 3.0f, 6.0f);
    view_dist       = std::min(view_dist, 10.0f); // Cap at reasonable distance

    // Position camera behind and above the object, closer to it
    // Camera should be at object's X, slightly above, and behind (higher Z)
    cam.position =
        glm::vec3(obj_pos.x,                    // Same X as object
                  obj_pos.y + view_dist * 0.6f, // Above object
                  obj_pos.z + view_dist // Behind object (further from origin)
        );

    // Clamp camera Z to reasonable range (15-30)
    cam.position.z = std::max(cam.position.z, 15.0f);
    cam.position.z = std::min(cam.position.z, 30.0f);

    // Calculate direction from camera to object
    glm::vec3 to_obj = obj_pos - cam.position;
    float     dist_horizontal =
        std::sqrt(to_obj.x * to_obj.x + to_obj.z * to_obj.z);

    // Calculate yaw: horizontal angle (0 = looking along +Z)
    cam.yaw = std::atan2(to_obj.x, to_obj.z) * 180.0f / 3.14159f;

    // Calculate pitch: vertical angle (negative = looking down)
    cam.pitch = std::atan2(-to_obj.y, dist_horizontal) * 180.0f / 3.14159f;
    cam.pitch = glm::clamp(cam.pitch, -89.0f, 89.0f);

    ui::log(2, "Focused camera on: " + obj.name);
}

void apply_sky()
{
    if (g_ctx && g_ctx->background)
    {
        g_ctx->background->r = ui::g_sky_color[0];
        g_ctx->background->g = ui::g_sky_color[1];
        g_ctx->background->b = ui::g_sky_color[2];
    }
}

void rebuild_grid()
{
    for (auto h : g_grids)
        if (h != euengine::invalid_mesh && g_ctx->renderer)
            g_ctx->renderer->destroy_mesh(h);
    g_grids.clear();

    // Infinite ground grid - very large size with many subdivisions
    // Main grid - lighter color
    g_grids.push_back(g_ctx->renderer->create_wireframe_grid(
        10000.0f,
        1000,
        { ui::g_grid_color[0], ui::g_grid_color[1], ui::g_grid_color[2] }));

    // Create infinite origin axis gizmo (RGB = XYZ like Godot)
    // Make lines very long to appear infinite, and slightly above grid to avoid
    // z-fighting
    if (g_origin_axis != euengine::invalid_mesh)
        g_ctx->renderer->destroy_mesh(g_origin_axis);

    const float axis_len = 10000.0f; // Very long to appear infinite
    const float axis_y_offset =
        0.01f; // Slightly above grid to avoid z-fighting
    const float axis_thickness = 0.05f; // Thickness of axis lines

    // Helper lambda to create a thick line segment as a quad
    auto add_thick_line = [&](const glm::vec3&               start,
                              const glm::vec3&               end,
                              const glm::vec3&               color,
                              const glm::vec3&               perp1,
                              const glm::vec3&               perp2,
                              std::vector<euengine::vertex>& verts,
                              std::vector<uint16_t>&         indices)
    {
        const glm::vec3 half_thick1 = perp1 * (axis_thickness * 0.5f);
        const glm::vec3 half_thick2 = perp2 * (axis_thickness * 0.5f);

        const uint16_t base_idx = static_cast<uint16_t>(verts.size());

        // Create quad vertices
        verts.push_back({ { start + half_thick1 + half_thick2 }, color });
        verts.push_back({ { start - half_thick1 + half_thick2 }, color });
        verts.push_back({ { start - half_thick1 - half_thick2 }, color });
        verts.push_back({ { start + half_thick1 - half_thick2 }, color });
        verts.push_back({ { end + half_thick1 + half_thick2 }, color });
        verts.push_back({ { end - half_thick1 + half_thick2 }, color });
        verts.push_back({ { end - half_thick1 - half_thick2 }, color });
        verts.push_back({ { end + half_thick1 - half_thick2 }, color });

        // Create quad indices (two triangles per quad)
        // Front face
        indices.push_back(base_idx + 0);
        indices.push_back(base_idx + 1);
        indices.push_back(base_idx + 2);
        indices.push_back(base_idx + 0);
        indices.push_back(base_idx + 2);
        indices.push_back(base_idx + 3);
        // Back face
        indices.push_back(base_idx + 4);
        indices.push_back(base_idx + 7);
        indices.push_back(base_idx + 6);
        indices.push_back(base_idx + 4);
        indices.push_back(base_idx + 6);
        indices.push_back(base_idx + 5);
        // Side faces
        indices.push_back(base_idx + 0);
        indices.push_back(base_idx + 4);
        indices.push_back(base_idx + 5);
        indices.push_back(base_idx + 0);
        indices.push_back(base_idx + 5);
        indices.push_back(base_idx + 1);
        indices.push_back(base_idx + 1);
        indices.push_back(base_idx + 5);
        indices.push_back(base_idx + 6);
        indices.push_back(base_idx + 1);
        indices.push_back(base_idx + 6);
        indices.push_back(base_idx + 2);
        indices.push_back(base_idx + 2);
        indices.push_back(base_idx + 6);
        indices.push_back(base_idx + 7);
        indices.push_back(base_idx + 2);
        indices.push_back(base_idx + 7);
        indices.push_back(base_idx + 3);
        indices.push_back(base_idx + 3);
        indices.push_back(base_idx + 7);
        indices.push_back(base_idx + 4);
        indices.push_back(base_idx + 3);
        indices.push_back(base_idx + 4);
        indices.push_back(base_idx + 0);
    };

    std::vector<euengine::vertex> axis_verts;
    std::vector<uint16_t>         axis_indices;

    // X axis - Red (positive and negative)
    const glm::vec3 x_start_pos = { 0.0f, axis_y_offset, 0.0f };
    const glm::vec3 x_end_pos   = { axis_len, axis_y_offset, 0.0f };
    const glm::vec3 x_end_neg   = { -axis_len, axis_y_offset, 0.0f };
    add_thick_line(x_start_pos,
                   x_end_pos,
                   { 1.0f, 0.0f, 0.0f },
                   { 0.0f, 1.0f, 0.0f },
                   { 0.0f, 0.0f, 1.0f },
                   axis_verts,
                   axis_indices);
    add_thick_line(x_start_pos,
                   x_end_neg,
                   { 1.0f, 0.0f, 0.0f },
                   { 0.0f, 1.0f, 0.0f },
                   { 0.0f, 0.0f, 1.0f },
                   axis_verts,
                   axis_indices);

    // Y axis - Green (positive and negative)
    const glm::vec3 y_start   = { 0.0f, 0.0f, 0.0f };
    const glm::vec3 y_end_pos = { 0.0f, axis_len, 0.0f };
    const glm::vec3 y_end_neg = { 0.0f, -axis_len, 0.0f };
    add_thick_line(y_start,
                   y_end_pos,
                   { 0.0f, 1.0f, 0.0f },
                   { 1.0f, 0.0f, 0.0f },
                   { 0.0f, 0.0f, 1.0f },
                   axis_verts,
                   axis_indices);
    add_thick_line(y_start,
                   y_end_neg,
                   { 0.0f, 1.0f, 0.0f },
                   { 1.0f, 0.0f, 0.0f },
                   { 0.0f, 0.0f, 1.0f },
                   axis_verts,
                   axis_indices);

    // Z axis - Blue (positive and negative)
    const glm::vec3 z_start   = { 0.0f, axis_y_offset, 0.0f };
    const glm::vec3 z_end_pos = { 0.0f, axis_y_offset, axis_len };
    const glm::vec3 z_end_neg = { 0.0f, axis_y_offset, -axis_len };
    add_thick_line(z_start,
                   z_end_pos,
                   { 0.0f, 0.0f, 1.0f },
                   { 1.0f, 0.0f, 0.0f },
                   { 0.0f, 1.0f, 0.0f },
                   axis_verts,
                   axis_indices);
    add_thick_line(z_start,
                   z_end_neg,
                   { 0.0f, 0.0f, 1.0f },
                   { 1.0f, 0.0f, 0.0f },
                   { 0.0f, 1.0f, 0.0f },
                   axis_verts,
                   axis_indices);

    g_origin_axis = g_ctx->renderer->create_mesh(
        axis_verts, axis_indices, euengine::primitive_type::triangles);
}

} // namespace scene
