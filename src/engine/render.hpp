#pragma once

#include "../shared/renderer_interface.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include <glm/glm.hpp>

#include <unordered_map>

namespace as3
{

class ShaderManager;

struct gpu_mesh final
{
    SDL_GPUBuffer* vertex_buffer = nullptr;
    SDL_GPUBuffer* index_buffer  = nullptr;
    Uint32         index_count   = 0;
    Uint32         vertex_count  = 0;
};

struct vertex_pos_color final
{
    glm::vec3 position;
    glm::vec3 color;
};

struct uniform_mvp final
{
    glm::mat4 view_proj;
};

class Renderer final : public IRenderer
{
public:
    Renderer() = default;
    ~Renderer() override;
    Renderer(const Renderer&)            = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&)                 = delete;
    Renderer& operator=(Renderer&&)      = delete;

    bool init(SDL_GPUDevice* device, ShaderManager* shaders);
    void shutdown();

    void begin_frame(SDL_GPUCommandBuffer* cmd, SDL_GPURenderPass* pass);
    void end_frame();

    // IRenderer interface
    void set_view_projection(const glm::mat4& vp) override;
    mesh_handle create_wireframe_cube(const glm::vec3& center, float size, const glm::vec3& color) override;
    mesh_handle create_wireframe_sphere(const glm::vec3& center, float radius, const glm::vec3& color, int segments) override;
    mesh_handle create_wireframe_grid(float size, int divisions, const glm::vec3& color) override;
    mesh_handle create_mesh(std::span<const vertex> vertices, std::span<const uint16_t> indices, primitive_type type) override;
    void destroy_mesh(mesh_handle mesh) override;
    void draw(mesh_handle mesh) override;

    void bind_pipeline();
    void reload_pipeline();

    [[nodiscard]] bool pipeline_valid() const { return pipeline_ != nullptr; }

private:
    bool create_pipeline();
    void draw_mesh_internal(const gpu_mesh& mesh);

    SDL_GPUDevice*           device_       = nullptr;
    ShaderManager*           shaders_      = nullptr;
    SDL_GPUGraphicsPipeline* pipeline_     = nullptr;
    SDL_GPURenderPass*       current_pass_ = nullptr;
    SDL_GPUCommandBuffer*    current_cmd_  = nullptr;
    uniform_mvp              uniforms_     = {};
    bool                     pipeline_dirty_ = false;

    std::unordered_map<mesh_handle, gpu_mesh> meshes_;
    mesh_handle next_handle_ = 1;
};

} // namespace as3
