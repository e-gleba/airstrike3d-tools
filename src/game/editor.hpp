#pragma once

#include "composite_model.hpp"
#include "renderer_interface.hpp"

#include <glm/glm.hpp>

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

struct ImGuiContext;

namespace as3
{

struct engine_context;

// Editable object wrapper
struct editable_object
{
    std::string      name;
    std::string      config_path;  // YAML config path for saving
    CompositeModel*  composite = nullptr;
    model_handle     model     = invalid_model;
    transform*       xform     = nullptr;
};

// Editor configuration
struct editor_config
{
    bool      show_bounds    = true;
    bool      show_grid      = true;
    bool      show_axes      = true;
    float     gizmo_scale    = 1.0f;
    glm::vec3 bounds_color   = { 0.0f, 1.0f, 0.0f };
    glm::vec3 selected_color = { 1.0f, 1.0f, 0.0f };
};

// File browser state
struct file_browser_state
{
    bool        open           = false;
    std::string title;
    std::string current_path;
    std::string selected_file;
    std::vector<std::string> extensions;  // e.g. {".obj", ".tga"}
    std::function<void(const std::string&)> on_select;
    
    // Directory contents cache
    std::vector<std::string> directories;
    std::vector<std::string> files;
    bool needs_refresh = true;
};

class Editor
{
public:
    Editor() = default;
    ~Editor() = default;

    void init(engine_context* ctx);
    void shutdown();

    // Register objects for editing
    void register_composite(const std::string& name, CompositeModel* model, 
                           transform* xform, const std::string& config_path = "");
    void register_model(const std::string& name, model_handle model, transform* xform);
    void clear_objects();

    // Update and render
    void update(float dt);
    void draw_gizmos();
    void draw_ui();

    // State
    [[nodiscard]] bool is_visible() const { return visible_; }
    void set_visible(bool v) { visible_ = v; }
    void toggle_visible() { visible_ = !visible_; }

    [[nodiscard]] int selected_index() const { return selected_idx_; }
    [[nodiscard]] editor_config& config() { return config_; }

    // File browser
    void open_file_browser(const std::string& title, const std::string& start_path,
                          const std::vector<std::string>& extensions,
                          std::function<void(const std::string&)> callback);

private:
    void draw_main_window();
    void draw_hierarchy_panel();
    void draw_inspector_panel();
    void draw_attachment_editor();
    void draw_asset_browser();
    void draw_file_browser_modal();

    void refresh_file_browser();
    
    // Gizmo drawing helpers
    void draw_bounds_gizmo(const bounds& b, const transform& xform, const glm::vec3& color);
    void draw_axis_gizmo(const glm::vec3& pos, float size);

    engine_context*              ctx_          = nullptr;
    std::vector<editable_object> objects_;
    int                          selected_idx_ = -1;
    int                          selected_attachment_ = -1;
    bool                         visible_      = false;
    editor_config                config_;
    file_browser_state           file_browser_;

    // Asset browser state
    std::string                  asset_browser_path_ = "assets";
    std::vector<std::string>     asset_dirs_;
    std::vector<std::string>     asset_files_;
    bool                         asset_browser_needs_refresh_ = true;

    // Wireframe gizmo meshes
    mesh_handle bounds_mesh_ = invalid_mesh;
    mesh_handle axis_mesh_   = invalid_mesh;
};

} // namespace as3

