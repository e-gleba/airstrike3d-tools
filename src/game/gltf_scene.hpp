#pragma once
/// @file gltf_scene.hpp
/// @brief Full glTF/GLB scene loader with Godot export support
/// Replaces old TSCN parser - use Godot's glTF export instead

#include <filesystem>
#include <functional>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <optional>
#include <string>
#include <vector>

namespace gltf_scene
{

/// Material data from glTF
struct scene_material
{
    std::string           name;
    glm::vec4             base_color { 1.0f };
    float                 metallic { 0.0f };
    float                 roughness { 1.0f };
    std::filesystem::path base_color_texture;
    std::filesystem::path normal_texture;
    std::filesystem::path metallic_roughness_texture;
    std::filesystem::path emissive_texture;
    glm::vec3             emissive_factor { 0.0f };
    bool                  double_sided { false };
    float                 alpha_cutoff { 0.5f };
    enum class alpha_mode
    {
        opaque,
        mask,
        blend
    } alpha { alpha_mode::opaque };
};

/// Node transform data
struct scene_transform
{
    glm::vec3 position { 0.0f };
    glm::quat rotation { 1.0f, 0.0f, 0.0f, 0.0f };
    glm::vec3 scale { 1.0f };

    [[nodiscard]] glm::mat4 matrix() const;
};

/// Scene node (mesh instance, light, camera, etc.)
struct scene_node
{
    std::string     name;
    scene_transform transform;
    int             mesh_index { -1 };     // Index into scene::meshes
    int             material_index { -1 }; // Index into scene::materials
    int             light_index { -1 };    // Index into scene::lights
    int             camera_index { -1 };   // Index into scene::cameras
    std::vector<int> children;             // Child node indices
    int              parent { -1 };        // Parent node index (-1 = root)

    // Computed world transform (populated during loading)
    glm::mat4 world_matrix { 1.0f };
};

/// Light data from glTF KHR_lights_punctual
struct scene_light
{
    std::string name;
    enum class type
    {
        directional,
        point,
        spot
    } light_type { type::point };
    glm::vec3 color { 1.0f };
    float     intensity { 1.0f };
    float     range { 0.0f };           // 0 = infinite
    float     inner_cone_angle { 0.0f }; // For spot lights
    float     outer_cone_angle { 0.78f }; // For spot lights (~45 degrees)
};

/// Camera data from glTF
struct scene_camera
{
    std::string name;
    bool        is_perspective { true };
    float       fov { 60.0f };     // Vertical FOV in degrees (perspective)
    float       aspect { 16.0f / 9.0f };
    float       near_plane { 0.1f };
    float       far_plane { 1000.0f };
    float       ortho_size { 10.0f }; // For orthographic cameras
};

/// Mesh primitive data
struct scene_primitive
{
    std::vector<glm::vec3>    positions;
    std::vector<glm::vec3>    normals;
    std::vector<glm::vec2>    texcoords;
    std::vector<glm::vec4>    colors;   // Vertex colors
    std::vector<glm::vec4>    tangents; // For normal mapping
    std::vector<uint32_t>     indices;
    int                       material_index { -1 };
};

/// Mesh data (can have multiple primitives)
struct scene_mesh
{
    std::string                   name;
    std::vector<scene_primitive>  primitives;
};

/// Environment/skybox data from Godot extensions
struct scene_environment
{
    glm::vec3             ambient_color { 0.1f };
    float                 ambient_intensity { 1.0f };
    std::filesystem::path skybox_texture;
    glm::vec3             fog_color { 0.5f, 0.6f, 0.7f };
    float                 fog_density { 0.0f };
    float                 fog_start { 10.0f };
    float                 fog_end { 100.0f };
};

/// Complete loaded scene
struct loaded_scene
{
    std::string                   name;
    std::filesystem::path         source_path;
    std::vector<scene_mesh>       meshes;
    std::vector<scene_material>   materials;
    std::vector<scene_node>       nodes;
    std::vector<scene_light>      lights;
    std::vector<scene_camera>     cameras;
    std::vector<int>              root_nodes; // Indices of root nodes
    scene_environment             environment;

    [[nodiscard]] bool empty() const { return nodes.empty(); }
};

/// Load options
struct load_options
{
    bool load_materials { true };
    bool load_lights { true };
    bool load_cameras { true };
    bool compute_world_transforms { true };
    bool flip_texcoords_v { false };       // Flip V coordinate for OpenGL
    float global_scale { 1.0f };           // Scale factor for all positions
    bool convert_coordinate_system { true }; // Convert from Godot to engine coords
};

/// Load result
struct load_result
{
    std::optional<loaded_scene> scene;
    std::string                 error;
    std::vector<std::string>    warnings;

    [[nodiscard]] explicit operator bool() const { return scene.has_value(); }
};

/// Load a glTF/GLB scene file
/// @param path Path to .gltf or .glb file
/// @param options Loading options
/// @return Load result with scene or error
[[nodiscard]] load_result load(const std::filesystem::path& path,
                               const load_options&          options = {});

/// Scan directory for glTF/GLB files
[[nodiscard]] std::vector<std::filesystem::path> scan_directory(
    const std::filesystem::path& directory,
    bool                         recursive = true);

} // namespace gltf_scene

