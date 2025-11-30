#pragma once

#include <glm/glm.hpp>

#include <cstdint>
#include <filesystem>
#include <span>

namespace as3
{

// Opaque handles
using mesh_handle = uint64_t;
using model_handle = uint64_t;
using texture_handle = uint64_t;

constexpr mesh_handle invalid_mesh = 0;
constexpr model_handle invalid_model = 0;
constexpr texture_handle invalid_texture = 0;

// Vertex format for custom meshes
struct vertex final
{
    glm::vec3 position;
    glm::vec3 color;
};

// Primitive types
enum class primitive_type : uint8_t
{
    lines,
    triangles,
};

// Render mode
enum class render_mode : uint8_t
{
    wireframe,
    textured,
    solid,  // textured without texture = solid color
};

// Transform for positioning models
struct transform final
{
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 rotation = glm::vec3(0.0f);  // Euler angles in degrees
    glm::vec3 scale    = glm::vec3(1.0f);
};

// Model bounds (AABB) for positioning child objects
struct bounds final
{
    glm::vec3 min = glm::vec3(0.0f);
    glm::vec3 max = glm::vec3(0.0f);
    [[nodiscard]] constexpr glm::vec3 center() const { return (min + max) * 0.5f; }
    [[nodiscard]] constexpr glm::vec3 size() const { return max - min; }
    [[nodiscard]] constexpr float height() const { return max.y - min.y; }
};

// Render statistics for debugging
struct render_stats final
{
    uint32_t draw_calls      = 0;
    uint32_t triangles       = 0;
    uint32_t vertices        = 0;
    uint32_t models_loaded   = 0;
    uint32_t textures_loaded = 0;
    uint32_t meshes_loaded   = 0;
};

// Abstract renderer interface
class IRenderer
{
public:
    virtual ~IRenderer() = default;
    
    virtual void set_view_projection(const glm::mat4& vp) = 0;
    
    // Render mode
    virtual void set_render_mode(render_mode mode) = 0;
    [[nodiscard]] virtual render_mode get_render_mode() const = 0;
    
    // Wireframe primitives
    virtual mesh_handle create_wireframe_cube(const glm::vec3& center, float size, const glm::vec3& color) = 0;
    virtual mesh_handle create_wireframe_sphere(const glm::vec3& center, float radius, const glm::vec3& color, int segments = 16) = 0;
    virtual mesh_handle create_wireframe_grid(float size, int divisions, const glm::vec3& color) = 0;
    
    // Custom mesh
    virtual mesh_handle create_mesh(std::span<const vertex> vertices, std::span<const uint16_t> indices, primitive_type type = primitive_type::lines) = 0;
    virtual void destroy_mesh(mesh_handle mesh) = 0;
    virtual void draw(mesh_handle mesh) = 0;
    
    // Model loading (OBJ with optional texture)
    virtual model_handle load_model(const std::filesystem::path& path, const glm::vec3& color = glm::vec3(1.0f)) = 0;
    virtual void unload_model(model_handle model) = 0;
    virtual void draw_model(model_handle model, const transform& xform) = 0;
    
    // Model bounds for attaching child objects (e.g., rotors on helicopters)
    [[nodiscard]] virtual bounds get_bounds(model_handle model) const = 0;
    
    // Texture loading
    virtual texture_handle load_texture(const std::filesystem::path& path) = 0;
    virtual void unload_texture(texture_handle tex) = 0;
    
    // Statistics
    [[nodiscard]] virtual render_stats get_stats() const = 0;
};

// Abstract shader manager interface
class IShaderManager
{
public:
    virtual ~IShaderManager() = default;
    virtual bool hot_reload_enabled() const = 0;
    virtual void enable_hot_reload(bool enable) = 0;
};

} // namespace as3
