#pragma once

/// @file scripting.hpp
/// @brief Lua scripting system for compound objects

#include "renderer_interface.hpp"

#include <glm/glm.hpp>
#include <sol/sol.hpp>

#include <deque>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace as3
{

/// A part of a compound object (turret, rotor, gun, etc.)
struct object_part
{
    std::string  name;
    std::string  model_path;
    model_handle model = invalid_model;

    // Local transform relative to parent
    glm::vec3 offset   = glm::vec3(0.0f);
    glm::vec3 rotation = glm::vec3(0.0f); // Euler degrees
    glm::vec3 scale    = glm::vec3(1.0f);

    // Rotation constraints
    glm::vec3 rotation_axis  = glm::vec3(0, 1, 0); // Y-axis default
    float     rotation_speed = 90.0f;              // Degrees per second
    float     min_angle      = -180.0f;
    float     max_angle      = 180.0f;
    bool      can_rotate     = false;
    bool      continuous     = false; // Continuous rotation (like rotor)

    // Runtime state
    float target_angle  = 0.0f;
    float current_angle = 0.0f;

    // Parent index (-1 = root)
    int parent_index = -1;
};

/// A compound object with multiple parts controlled by Lua
struct compound_object
{
    std::string script_path;
    std::string name;

    // Parts hierarchy
    std::vector<object_part> parts;

    // World transform
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 rotation = glm::vec3(0.0f);
    glm::vec3 scale    = glm::vec3(1.0f);

    // Selection
    bool  selected         = false;
    float selection_radius = 5.0f; // For easier selection

    // Movement
    float     move_speed    = 10.0f;
    float     turn_speed    = 90.0f;
    glm::vec3 target_pos    = glm::vec3(0.0f);
    bool      has_target    = false;
    bool      can_move      = true;
    float     current_speed = 0.0f;

    // Script state key for Lua table
    std::string state_key;
};

/// Log entry for script errors/output
struct script_log_entry
{
    enum class level
    {
        info,
        warning,
        error
    };
    level       type;
    std::string message;
    std::string source;
};

/// Lua scripting manager
class ScriptManager
{
public:
    ScriptManager();
    ~ScriptManager();

    // Non-copyable
    ScriptManager(const ScriptManager&)            = delete;
    ScriptManager& operator=(const ScriptManager&) = delete;

    /// Initialize with renderer
    void init(IRenderer* renderer);

    /// Shutdown and cleanup
    void shutdown();

    /// Load a compound object from Lua script
    [[nodiscard]] bool load_object(compound_object&             obj,
                                   const std::filesystem::path& script);

    /// Reload a script (hot-reload)
    [[nodiscard]] bool reload_object(compound_object& obj);

    /// Update object (call Lua update function)
    void update_object(compound_object& obj, float dt);

    /// Called when object is selected
    void on_select(compound_object& obj);

    /// Called when object is deselected
    void on_deselect(compound_object& obj);

    /// Called when object receives move command
    void on_move_command(compound_object& obj, const glm::vec3& target);

    /// Get script log
    [[nodiscard]] const std::deque<script_log_entry>& get_log() const
    {
        return log_;
    }

    /// Clear log
    void clear_log() { log_.clear(); }

    /// Draw ImGui log window
    void draw_log_window(bool* open);

    /// Check if there are errors
    [[nodiscard]] bool has_errors() const { return error_count_ > 0; }

    /// Get error count
    [[nodiscard]] int error_count() const { return error_count_; }

private:
    void setup_lua_api();
    void log_message(script_log_entry::level lvl,
                     const std::string&      msg,
                     const std::string&      src = "");
    void handle_lua_error(const sol::error& e, const std::string& context);

    sol::state                   lua_;
    IRenderer*                   renderer_ = nullptr;
    std::deque<script_log_entry> log_;
    int                          error_count_ = 0;

    static constexpr std::size_t k_max_log_entries = 100;
};

} // namespace as3
