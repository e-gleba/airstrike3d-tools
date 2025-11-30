#pragma once

#include "renderer_interface.hpp"

#include <glm/glm.hpp>

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace as3
{

// Level object placement
struct level_object
{
    std::string type_name;
    glm::vec3   position = glm::vec3(0.0f);
    float       rotation = 0.0f;
    int         flags    = 0;
};

// Level header info
struct level_header
{
    std::string name;
    int         grid_size      = 32;
    int         terrain_scale  = 256;
    int         object_count   = 0;
    int         heightmap_layers = 0;
};

// Object type to model path mapping
struct object_type_mapping
{
    std::string type_name;
    std::string model_path;
    std::string texture_path;
    float       scale = 1.0f;
};

// Loaded level data
class Level
{
public:
    Level() = default;
    ~Level() = default;

    // Load level from YAML file
    bool load(const std::filesystem::path& yaml_path);
    void unload();

    // Accessors
    [[nodiscard]] bool is_loaded() const { return loaded_; }
    [[nodiscard]] const level_header& header() const { return header_; }
    [[nodiscard]] const std::vector<std::string>& object_types() const { return object_types_; }
    [[nodiscard]] const std::vector<std::string>& item_types() const { return item_types_; }
    [[nodiscard]] const std::vector<level_object>& objects() const { return objects_; }
    [[nodiscard]] const std::vector<std::vector<int>>& heightmap() const { return combined_heightmap_; }

    // Get world-space bounds of level
    [[nodiscard]] glm::vec3 get_world_size() const;
    [[nodiscard]] float get_height_at(float x, float y) const;

private:
    bool              loaded_ = false;
    level_header      header_;
    std::vector<std::string>           object_types_;
    std::vector<std::string>           item_types_;
    std::vector<level_object>          objects_;
    std::vector<std::vector<int>>      combined_heightmap_;  // y rows of x values
};

// Level manager - handles loading and rendering levels
class LevelManager
{
public:
    LevelManager() = default;
    ~LevelManager() = default;

    void init(IRenderer* renderer);
    void shutdown();

    // Level loading
    bool load_level(const std::filesystem::path& yaml_path);
    void unload_level();

    // Get available levels
    std::vector<std::string> list_levels(const std::filesystem::path& maps_dir);

    // Render current level objects
    void render();

    // Accessors
    [[nodiscard]] bool has_level() const { return level_.is_loaded(); }
    [[nodiscard]] const Level& level() const { return level_; }
    [[nodiscard]] Level& level() { return level_; }

    // Object type mappings (type_name -> model info)
    void register_object_mapping(const object_type_mapping& mapping);
    void load_default_mappings();

private:
    struct loaded_level_model
    {
        model_handle model = invalid_model;
        float        scale = 1.0f;
    };

    IRenderer*         renderer_ = nullptr;
    Level              level_;
    
    // Type name -> mapping
    std::unordered_map<std::string, object_type_mapping> type_mappings_;
    
    // Loaded models for this level
    std::unordered_map<std::string, loaded_level_model> loaded_models_;

    // Terrain mesh for heightmap
    mesh_handle terrain_mesh_ = invalid_mesh;

    void load_level_models();
    void unload_level_models();
    void create_terrain_mesh();
};

} // namespace as3

