#pragma once

#include "window.hpp"

#include <glm/glm.hpp>

#include <cstdint>
#include <filesystem>
#include <span>

namespace euengine
{

// Forward declaration
class i_engine_settings;

using mesh_handle    = uint64_t;
using model_handle   = uint64_t;
using texture_handle = uint64_t;

constexpr mesh_handle    invalid_mesh    = 0;
constexpr model_handle   invalid_model   = 0;
constexpr texture_handle invalid_texture = 0;

struct vertex final
{
    glm::vec3 position;
    glm::vec3 color;
};

enum class primitive_type : uint8_t
{
    lines,
    triangles,
};

enum class render_mode : uint8_t
{
    wireframe,
    textured,
    solid, // textured without texture = solid color
};

struct transform final
{
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 rotation = glm::vec3(0.0f); // Euler angles in degrees
    glm::vec3 scale    = glm::vec3(1.0f);
};

struct bounds final
{
    glm::vec3                         min = glm::vec3(0.0f);
    glm::vec3                         max = glm::vec3(0.0f);
    [[nodiscard]] constexpr glm::vec3 center() const
    {
        return (min + max) * 0.5f;
    }
    [[nodiscard]] constexpr glm::vec3 size() const { return max - min; }
    [[nodiscard]] constexpr float     height() const { return max.y - min.y; }
};

struct render_stats final
{
    uint32_t draw_calls      = 0;
    uint32_t triangles       = 0;
    uint32_t vertices        = 0;
    uint32_t models_loaded   = 0;
    uint32_t textures_loaded = 0;
    uint32_t meshes_loaded   = 0;
};

class i_renderer
{
public:
    virtual ~i_renderer() = default;

    virtual void set_view_projection(const glm::mat4& vp) = 0;

    virtual void                      set_render_mode(render_mode mode) = 0;
    [[nodiscard]] virtual render_mode get_render_mode() const           = 0;

    virtual mesh_handle create_wireframe_cube(const glm::vec3& center,
                                              float            size,
                                              const glm::vec3& color) = 0;
    virtual mesh_handle create_wireframe_sphere(const glm::vec3& center,
                                                float            radius,
                                                const glm::vec3& color,
                                                int segments = 16)    = 0;
    virtual mesh_handle create_wireframe_grid(float            size,
                                              int              divisions,
                                              const glm::vec3& color) = 0;

    virtual mesh_handle create_mesh(
        std::span<const vertex>   vertices,
        std::span<const uint16_t> indices,
        primitive_type            type = primitive_type::lines) = 0;
    virtual void destroy_mesh(mesh_handle mesh)      = 0;
    virtual void draw(mesh_handle mesh)              = 0;

    virtual model_handle load_model(
        const std::filesystem::path& path,
        const glm::vec3&             color = glm::vec3(1.0f))                       = 0;
    virtual void unload_model(model_handle model)                       = 0;
    virtual void draw_model(model_handle model, const transform& xform) = 0;

    [[nodiscard]] virtual bounds get_bounds(model_handle model) const = 0;

    virtual void draw_bounds(const bounds&    b,
                             const transform& xform,
                             const glm::vec3& color = glm::vec3(0.0f,
                                                                1.0f,
                                                                0.0f)) = 0;

    virtual texture_handle load_texture(const std::filesystem::path& path) = 0;
    virtual void           unload_texture(texture_handle tex)              = 0;

    virtual void set_msaa_samples(msaa_samples samples) = 0;
    virtual void set_max_anisotropy(float anisotropy)   = 0;

    enum class texture_filter : std::uint8_t
    {
        nearest   = 0,
        linear    = 1,
        trilinear = 2,
    };
    virtual void set_texture_filter(texture_filter filter) = 0;

    [[nodiscard]] virtual render_stats get_stats() const = 0;
};

class i_shader_manager
{
public:
    virtual ~i_shader_manager()                 = default;
    virtual bool hot_reload_enabled() const     = 0;
    virtual void enable_hot_reload(bool enable) = 0;
};

} // namespace euengine
