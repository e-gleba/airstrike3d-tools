#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include <glm/glm.hpp>

namespace as3
{

class ShaderManager;

struct GPUMesh
{
    SDL_GPUBuffer* vertex_buffer   = nullptr;
    SDL_GPUBuffer* index_buffer    = nullptr;
    Uint32         index_count     = 0;
    Uint32         vertex_count    = 0;
};

struct VertexPosColor
{
    glm::vec3 position;
    glm::vec3 color;
};

struct UniformMVP
{
    glm::mat4 view_proj;
};

class Renderer
{
public:
    Renderer()                           = default;
    ~Renderer();
    Renderer(const Renderer&)            = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&)                 = delete;
    Renderer& operator=(Renderer&&)      = delete;

    bool init(SDL_GPUDevice* device, ShaderManager* shaders);
    void shutdown();

    void begin_frame(SDL_GPUCommandBuffer* cmd);
    void end_frame();

    void set_view_projection(const glm::mat4& vp);
    void bind_pipeline(SDL_GPURenderPass* pass);
    void draw_mesh(SDL_GPURenderPass* pass, SDL_GPUCommandBuffer* cmd, const GPUMesh& mesh);

    [[nodiscard]] bool pipeline_valid() const { return pipeline_ != nullptr; }

    void reload_pipeline();

private:
    bool create_pipeline();

    SDL_GPUDevice*           device_          = nullptr;
    ShaderManager*           shaders_         = nullptr;
    SDL_GPUGraphicsPipeline* pipeline_        = nullptr;
    UniformMVP               uniforms_        = {};
    bool                     pipeline_dirty_  = false;
};

// Utility functions for mesh creation
[[nodiscard]] GPUMesh create_wireframe_cube(SDL_GPUDevice* device,
                                            const glm::vec3& center,
                                            float            size,
                                            const glm::vec3& color);

void destroy_mesh(SDL_GPUDevice* device, GPUMesh& mesh);

} // namespace as3
