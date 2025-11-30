#pragma once

#include "renderer_interface.hpp"
#include "unit.hpp"

#include <glm/glm.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace as3
{

// Scene configuration
struct scene_config
{
    std::string name          = "Untitled";
    glm::vec3   ambient_color = glm::vec3(0.3f);
    glm::vec3   sun_direction = glm::vec3(-0.5f, -1.0f, -0.5f);
    glm::vec3   sun_color     = glm::vec3(1.0f, 0.95f, 0.8f);

    std::vector<unit_data> units;
};

// Scene manager
class Scene
{
public:
    Scene()  = default;
    ~Scene() = default;

    void init(IRenderer* renderer);
    void shutdown();

    // File operations
    bool load(const std::filesystem::path& filepath);
    bool save(const std::filesystem::path& filepath) const;
    bool save() const;
    void new_scene(const std::string& name = "Untitled");

    // Update (movement, physics)
    void update(float dt);

    // Render
    void render();

    // Unit management
    unit_data& add_unit(const std::string& name,
                        unit_type          type = unit_type::static_object);
    void       remove_unit(size_t index);
    void       duplicate_unit(size_t index);

    // Selection
    void                     select_unit(size_t index);
    void                     deselect_all();
    [[nodiscard]] int        get_selected_index() const;
    [[nodiscard]] unit_data* get_selected();

    // Movement commands
    void command_move_to(const glm::vec3& target);
    void command_stop();

    // Accessors
    [[nodiscard]] bool is_dirty() const { return dirty_; }
    [[nodiscard]] bool has_file() const { return !current_path_.empty(); }
    [[nodiscard]] const std::filesystem::path& current_path() const
    {
        return current_path_;
    }
    [[nodiscard]] scene_config&           config() { return config_; }
    [[nodiscard]] const scene_config&     config() const { return config_; }
    [[nodiscard]] std::vector<unit_data>& units() { return config_.units; }
    [[nodiscard]] const std::vector<unit_data>& units() const
    {
        return config_.units;
    }

    // List available scenes
    [[nodiscard]] static std::vector<std::string> list_scenes(
        const std::filesystem::path& dir);

private:
    IRenderer*            renderer_ = nullptr;
    scene_config          config_;
    std::filesystem::path current_path_;
    bool                  dirty_          = false;
    int                   selected_index_ = -1;

    void load_unit_model(unit_data& unit);
    void unload_unit_model(unit_data& unit);
};

} // namespace as3
