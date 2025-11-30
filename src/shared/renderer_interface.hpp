#pragma once

#include <glm/glm.hpp>

#include <cstdint>
#include <span>

namespace as3
{

// Opaque mesh handle - game doesn't need to know internals
using mesh_handle = uint64_t;
constexpr mesh_handle invalid_mesh = 0;

// Vertex format for custom meshes
struct vertex final
{
    glm::vec3 position;
    glm::vec3 color;
};

// Primitive types for mesh creation
enum class primitive_type : uint8_t
{
    lines,
    triangles,
};

// Abstract renderer interface - no SDL deps
class IRenderer
{
public:
    virtual ~IRenderer() = default;
    
    virtual void set_view_projection(const glm::mat4& vp) = 0;
    
    // Mesh management - primitives
    virtual mesh_handle create_wireframe_cube(const glm::vec3& center,
                                              float size,
                                              const glm::vec3& color) = 0;
    virtual mesh_handle create_wireframe_sphere(const glm::vec3& center,
                                                float radius,
                                                const glm::vec3& color,
                                                int segments = 16) = 0;
    virtual mesh_handle create_wireframe_grid(float size, int divisions,
                                              const glm::vec3& color) = 0;
    
    // Custom mesh from vertices/indices
    virtual mesh_handle create_mesh(std::span<const vertex> vertices,
                                    std::span<const uint16_t> indices,
                                    primitive_type type = primitive_type::lines) = 0;
    
    virtual void destroy_mesh(mesh_handle mesh) = 0;
    virtual void draw(mesh_handle mesh) = 0;
};

// Abstract shader manager interface - no SDL deps
class IShaderManager
{
public:
    virtual ~IShaderManager() = default;
    virtual bool hot_reload_enabled() const = 0;
    virtual void enable_hot_reload(bool enable) = 0;
};

} // namespace as3

