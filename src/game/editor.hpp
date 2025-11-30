#pragma once

#include "audio_interface.hpp"
#include "renderer_interface.hpp"
#include "scene.hpp"

#include <glm/glm.hpp>

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

struct ImGuiContext;

namespace as3
{

struct engine_context;

// Editor configuration
struct editor_config
{
    bool      show_bounds      = true;
    bool      show_grid        = true;
    bool      show_axes        = true;
    bool      show_move_target = true;
    float     gizmo_scale      = 1.0f;
    glm::vec3 bounds_color     = { 0.2f, 0.8f, 0.2f };
    glm::vec3 selected_color   = { 1.0f, 0.8f, 0.2f };
    glm::vec3 target_color     = { 1.0f, 0.3f, 0.3f };
};

// File browser state
struct file_browser_state
{
    bool                                    open = false;
    std::string                             title;
    std::string                             current_path;
    std::string                             selected_file;
    std::vector<std::string>                extensions;
    std::function<void(const std::string&)> on_select;

    std::vector<std::string> directories;
    std::vector<std::string> files;
    bool                     needs_refresh = true;
};

class Editor
{
public:
    Editor()  = default;
    ~Editor() = default;

    void init(engine_context* ctx);
    void shutdown();

    [[nodiscard]] Scene&       scene() { return scene_; }
    [[nodiscard]] const Scene& scene() const { return scene_; }

    void update(float dt);
    void draw_gizmos();
    void draw_ui();

    [[nodiscard]] bool is_visible() const { return visible_; }
    void               set_visible(bool v) { visible_ = v; }
    void               toggle_visible() { visible_ = !visible_; }

    [[nodiscard]] editor_config& config() { return config_; }

    void create_default_scene();

    // Command selected unit to move
    void                    set_move_target(const glm::vec3& target);
    [[nodiscard]] bool      has_move_target() const { return move_target_set_; }
    [[nodiscard]] glm::vec3 get_move_target() const { return move_target_; }

    // Mouse interaction
    void handle_click(const glm::vec3& world_pos, bool is_right_click);
    void try_select_at(const glm::vec3& world_pos);

    // Selection mode
    enum class click_mode
    {
        select,
        move_command
    };
    [[nodiscard]] click_mode get_click_mode() const { return click_mode_; }
    void                     set_click_mode(click_mode m) { click_mode_ = m; }

private:
    void draw_main_window();
    void draw_scene_panel();
    void draw_units_panel();
    void draw_inspector_panel();
    void draw_audio_panel();
    void draw_file_browser_modal();

    void refresh_file_browser();
    void open_file_browser(const std::string&                      title,
                           const std::string&                      start_path,
                           const std::vector<std::string>&         extensions,
                           std::function<void(const std::string&)> callback);

    void draw_bounds_gizmo(const bounds&    b,
                           const transform& xform,
                           const glm::vec3& color);
    void draw_axis_gizmo(const glm::vec3& pos, float size);
    void draw_target_marker(const glm::vec3& pos);

    engine_context*    ctx_ = nullptr;
    Scene              scene_;
    bool               visible_ = true;
    editor_config      config_;
    file_browser_state file_browser_;

    // Scene browser
    std::vector<std::string> scene_list_;
    int                      selected_scene_ = -1;

    // Audio
    std::vector<std::string>  music_files_;
    std::vector<music_handle> loaded_music_;
    int                       selected_music_ = -1;
    int                       current_music_  = -1;

    std::vector<std::string>  sound_files_;
    std::vector<sound_handle> loaded_sounds_;
    int                       selected_sound_ = -1;

    // Movement target
    glm::vec3 move_target_     = glm::vec3(0.0f);
    bool      move_target_set_ = false;

    // Click mode
    click_mode click_mode_ = click_mode::select;

    // Gizmo meshes
    mesh_handle bounds_mesh_ = invalid_mesh;
    mesh_handle axis_mesh_   = invalid_mesh;
    mesh_handle target_mesh_ = invalid_mesh;
};

} // namespace as3
