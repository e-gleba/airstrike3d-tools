#pragma once

/// @file render.hpp
/// @brief GPU renderer with mesh and model management

#include "loaders/model_loader_registry.hpp"
#include "renderer_interface.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include <glm/glm.hpp>

#include <string>
#include <unordered_map>
#include <vector>

namespace as3
{

// Forward declarations
class ShaderManager;

/// GPU mesh data for wireframe rendering
struct gpu_mesh final
{
    SDL_GPUBuffer* vertex_buffer = nullptr;
    SDL_GPUBuffer* index_buffer  = nullptr;
    Uint32         index_count   = 0;
    Uint32         vertex_count  = 0;
};

/// GPU texture with sampler
struct gpu_texture final
{
    SDL_GPUTexture* texture = nullptr;
    SDL_GPUSampler* sampler = nullptr;
    std::int32_t    width   = 0;
    std::int32_t    height  = 0;
};

/// Textured mesh for model rendering
struct gpu_textured_mesh final
{
    SDL_GPUBuffer* vertex_buffer = nullptr;
    SDL_GPUBuffer* index_buffer  = nullptr;
    Uint32         index_count   = 0;
    Uint32         vertex_count  = 0;
};

/// Complete GPU model with meshes, texture, and bounds
struct gpu_model final
{
    std::vector<gpu_textured_mesh> meshes;
    texture_handle                 texture      = invalid_texture;
    glm::vec3                      color        = glm::vec3(1.0f);
    bounds                         model_bounds = {};
    bool                           has_uvs      = false;
};

/// Vertex with position and color (wireframe)
struct vertex_pos_color final
{
    glm::vec3 position;
    glm::vec3 color;
};

/// Vertex with position, normal, and texture coordinates
struct vertex_textured final
{
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texcoord;
};

/// MVP uniform data for shaders
struct uniform_mvp final
{
    glm::mat4 mvp;
};

/// Main renderer class implementing IRenderer interface
class Renderer final : public IRenderer
{
public:
    Renderer() = default;
    ~Renderer() override;

    // Non-copyable, non-movable
    Renderer(const Renderer&)            = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&)                 = delete;
    Renderer& operator=(Renderer&&)      = delete;

    /// Initialize renderer with GPU device and shader manager
    [[nodiscard]] bool init(SDL_GPUDevice* device, ShaderManager* shaders);

    /// Release all GPU resources
    void shutdown();

    /// Begin a new frame
    void begin_frame(SDL_GPUCommandBuffer* cmd, SDL_GPURenderPass* pass);

    /// End current frame
    void end_frame();

    /// Ensure depth texture matches given dimensions
    void ensure_depth_texture(Uint32 width, Uint32 height);

    /// Get current depth texture
    [[nodiscard]] SDL_GPUTexture* depth_texture() const noexcept
    {
        return depth_texture_;
    }

    // IRenderer interface implementation
    void                      set_view_projection(const glm::mat4& vp) override;
    void                      set_render_mode(render_mode mode) override;
    [[nodiscard]] render_mode get_render_mode() const noexcept override
    {
        return render_mode_;
    }

    // Wireframe primitives
    mesh_handle create_wireframe_cube(const glm::vec3& center,
                                      float            size,
                                      const glm::vec3& color) override;
    mesh_handle create_wireframe_sphere(const glm::vec3& center,
                                        float            radius,
                                        const glm::vec3& color,
                                        int              segments) override;
    mesh_handle create_wireframe_grid(float            size,
                                      int              divisions,
                                      const glm::vec3& color) override;

    // Custom mesh management
    mesh_handle create_mesh(std::span<const vertex>   vertices,
                            std::span<const uint16_t> indices,
                            primitive_type            type) override;
    void        destroy_mesh(mesh_handle mesh) override;
    void        draw(mesh_handle mesh) override;

    // Model management
    model_handle load_model(const std::filesystem::path& path,
                            const glm::vec3&             color) override;
    void         unload_model(model_handle model) override;
    void draw_model(model_handle model, const transform& xform) override;
    [[nodiscard]] bounds get_bounds(model_handle model) const override;

    // Debug drawing
    void draw_bounds(const bounds&    b,
                     const transform& xform,
                     const glm::vec3& color) override;

    // Texture management
    texture_handle load_texture(const std::filesystem::path& path) override;
    void           unload_texture(texture_handle tex) override;

    // Statistics
    [[nodiscard]] render_stats get_stats() const noexcept override;

    /// Bind default pipeline (called at frame start)
    void bind_pipeline();

    /// Rebuild pipelines if shaders changed
    void reload_pipelines();

    /// Check if wireframe pipeline is valid
    [[nodiscard]] bool pipeline_valid() const noexcept
    {
        return wireframe_pipeline_ != nullptr;
    }

private:
    [[nodiscard]] bool create_wireframe_pipeline();
    [[nodiscard]] bool create_textured_pipeline();

    void draw_mesh_internal(const gpu_mesh& mesh);
    void draw_textured_mesh_internal(const gpu_textured_mesh& mesh,
                                     texture_handle           tex);

    [[nodiscard]] gpu_mesh upload_wireframe_mesh(
        std::span<const vertex_pos_color> vertices,
        std::span<const uint16_t>         indices);
    [[nodiscard]] gpu_textured_mesh upload_textured_mesh(
        std::span<const vertex_textured> vertices,
        std::span<const uint16_t>        indices);

    [[nodiscard]] std::filesystem::path find_texture_for_model(
        const std::filesystem::path& model_path);

    /// Convert loaded model data to GPU model
    [[nodiscard]] gpu_model upload_loaded_model(const loaded_model& data,
                                                const glm::vec3&    color);

    // GPU device (non-owning)
    SDL_GPUDevice* device_  = nullptr;
    ShaderManager* shaders_ = nullptr;

    // Pipelines
    SDL_GPUGraphicsPipeline* wireframe_pipeline_          = nullptr;
    SDL_GPUGraphicsPipeline* textured_pipeline_           = nullptr;
    SDL_GPUGraphicsPipeline* textured_wireframe_pipeline_ = nullptr;

    // Current frame state
    SDL_GPURenderPass*    current_pass_ = nullptr;
    SDL_GPUCommandBuffer* current_cmd_  = nullptr;

    // Render state
    glm::mat4   view_proj_      = glm::mat4(1.0f);
    render_mode render_mode_    = render_mode::wireframe;
    bool        pipeline_dirty_ = false;

    // Resource maps
    std::unordered_map<mesh_handle, gpu_mesh>       meshes_;
    std::unordered_map<model_handle, gpu_model>     models_;
    std::unordered_map<texture_handle, gpu_texture> textures_;

    // Default white texture for untextured models
    texture_handle default_texture_ = invalid_texture;

    // Depth buffer
    SDL_GPUTexture* depth_texture_ = nullptr;
    Uint32          depth_width_   = 0;
    Uint32          depth_height_  = 0;

    // Handle generators
    std::uint64_t next_mesh_handle_    = 1;
    std::uint64_t next_model_handle_   = 1;
    std::uint64_t next_texture_handle_ = 1;

    // Temporary meshes for debug drawing (cleaned up each frame)
    std::vector<gpu_mesh> temp_meshes_;

    // Per-frame statistics
    mutable render_stats frame_stats_{};
};

} // namespace as3
