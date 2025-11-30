#include "render.hpp"

#include "shader.hpp"

#include <spdlog/spdlog.h>

#include <array>
#include <cstring>
#include <vector>

namespace as3
{

namespace
{
constexpr std::array<glm::vec3, 8> k_cube_offsets = { {
    glm::vec3(-1.0f, -1.0f, -1.0f),
    glm::vec3(1.0f, -1.0f, -1.0f),
    glm::vec3(1.0f, 1.0f, -1.0f),
    glm::vec3(-1.0f, 1.0f, -1.0f),
    glm::vec3(-1.0f, -1.0f, 1.0f),
    glm::vec3(1.0f, -1.0f, 1.0f),
    glm::vec3(1.0f, 1.0f, 1.0f),
    glm::vec3(-1.0f, 1.0f, 1.0f),
} };

constexpr std::array<std::pair<size_t, size_t>, 12> k_cube_edges = { {
    { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 },
    { 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 },
    { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 },
} };
} // namespace

Renderer::~Renderer()
{
    shutdown();
}

bool Renderer::init(SDL_GPUDevice* device, ShaderManager* shaders)
{
    device_  = device;
    shaders_ = shaders;

    // Load wireframe shader
    const ShaderProgramDesc wireframe_desc{
        .name     = "wireframe",
        .vertex   = { .path = "wireframe.vert.hlsl", .stage = ShaderStage::Vertex },
        .fragment = { .path = "wireframe.frag.hlsl", .stage = ShaderStage::Fragment },
    };

    auto result = shaders_->load_program(wireframe_desc);
    if (!result)
    {
        spdlog::error("Failed to load wireframe shader: {}", result.error());
        return false;
    }

    shaders_->set_reload_callback(
        [this](const std::string& name)
        {
            if (name == "wireframe")
            {
                pipeline_dirty_ = true;
            }
        });

    return create_pipeline();
}

void Renderer::shutdown()
{
    if (device_ && pipeline_)
    {
        SDL_ReleaseGPUGraphicsPipeline(device_, pipeline_);
        pipeline_ = nullptr;
    }
}

bool Renderer::create_pipeline()
{
    auto* program = shaders_->get_program("wireframe");
    if (!program || !program->valid())
    {
        spdlog::error("Invalid shader program for pipeline");
        return false;
    }

    const SDL_GPUVertexAttribute vertex_attrs[2] = {
        { .location = 0, .buffer_slot = 0,
          .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = 0 },
        { .location = 1, .buffer_slot = 0,
          .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
          .offset = sizeof(glm::vec3) },
    };

    const SDL_GPUVertexBufferDescription vb_desc{
        .slot = 0, .pitch = sizeof(VertexPosColor),
        .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX, .instance_step_rate = 0,
    };

    const SDL_GPUVertexInputState vertex_input{
        .vertex_buffer_descriptions = &vb_desc,
        .num_vertex_buffers         = 1,
        .vertex_attributes          = vertex_attrs,
        .num_vertex_attributes      = 2,
    };

    const SDL_GPUColorTargetDescription color_desc{
        .format = SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM, .blend_state = {},
    };

    const SDL_GPUGraphicsPipelineTargetInfo target_info{
        .color_target_descriptions = &color_desc,
        .num_color_targets         = 1,
        .depth_stencil_format      = SDL_GPU_TEXTUREFORMAT_INVALID,
        .has_depth_stencil_target  = false,
        .padding1                  = {},
        .padding2                  = {},
        .padding3                  = {},
    };

    const SDL_GPURasterizerState raster{
        .fill_mode                  = SDL_GPU_FILLMODE_FILL,
        .cull_mode                  = SDL_GPU_CULLMODE_NONE,
        .front_face                 = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE,
        .depth_bias_constant_factor = 0.0f,
        .depth_bias_clamp           = 0.0f,
        .depth_bias_slope_factor    = 0.0f,
        .enable_depth_bias          = false,
        .enable_depth_clip          = false,
        .padding1                   = {},
        .padding2                   = {},
    };

    const SDL_GPUMultisampleState ms{
        .sample_count             = SDL_GPU_SAMPLECOUNT_1,
        .sample_mask              = 0,
        .enable_mask              = false,
        .enable_alpha_to_coverage = false,
        .padding2                 = {},
        .padding3                 = {},
    };

    const SDL_GPUGraphicsPipelineCreateInfo info{
        .vertex_shader       = program->vertex_shader(),
        .fragment_shader     = program->fragment_shader(),
        .vertex_input_state  = vertex_input,
        .primitive_type      = SDL_GPU_PRIMITIVETYPE_LINELIST,
        .rasterizer_state    = raster,
        .multisample_state   = ms,
        .depth_stencil_state = {},
        .target_info         = target_info,
        .props               = 0,
    };

    if (pipeline_)
    {
        SDL_ReleaseGPUGraphicsPipeline(device_, pipeline_);
    }

    pipeline_ = SDL_CreateGPUGraphicsPipeline(device_, &info);
    if (!pipeline_)
    {
        spdlog::error("Failed to create pipeline: {}", SDL_GetError());
        return false;
    }

    pipeline_dirty_ = false;
    spdlog::info("Pipeline created successfully");
    return true;
}

void Renderer::reload_pipeline()
{
    if (pipeline_dirty_)
    {
        create_pipeline();
    }
}

void Renderer::begin_frame([[maybe_unused]] SDL_GPUCommandBuffer* cmd)
{
    reload_pipeline();
}

void Renderer::end_frame()
{
}

void Renderer::set_view_projection(const glm::mat4& vp)
{
    uniforms_.view_proj = vp;
}

void Renderer::bind_pipeline(SDL_GPURenderPass* pass)
{
    if (pipeline_)
    {
        SDL_BindGPUGraphicsPipeline(pass, pipeline_);
    }
}

void Renderer::draw_mesh(SDL_GPURenderPass*    pass,
                         SDL_GPUCommandBuffer* cmd,
                         const GPUMesh&        mesh)
{
    if (!pipeline_)
        return;

    SDL_PushGPUVertexUniformData(cmd, 0, &uniforms_, sizeof(UniformMVP));

    const SDL_GPUBufferBinding vb_binding{ .buffer = mesh.vertex_buffer, .offset = 0 };
    SDL_BindGPUVertexBuffers(pass, 0, &vb_binding, 1);

    const SDL_GPUBufferBinding ib_binding{ .buffer = mesh.index_buffer, .offset = 0 };
    SDL_BindGPUIndexBuffer(pass, &ib_binding, SDL_GPU_INDEXELEMENTSIZE_16BIT);

    SDL_DrawGPUIndexedPrimitives(pass, mesh.index_count, 1, 0, 0, 0);
}

GPUMesh create_wireframe_cube(SDL_GPUDevice*   device,
                              const glm::vec3& center,
                              float            size,
                              const glm::vec3& color)
{
    const float half = size * 0.5f;

    std::vector<VertexPosColor> vertices;
    vertices.reserve(8);
    for (const auto& offset : k_cube_offsets)
    {
        vertices.push_back({ .position = center + offset * half, .color = color });
    }

    std::vector<Uint16> indices;
    indices.reserve(24);
    for (const auto& [i, j] : k_cube_edges)
    {
        indices.push_back(static_cast<Uint16>(i));
        indices.push_back(static_cast<Uint16>(j));
    }

    GPUMesh mesh{};
    mesh.vertex_count = static_cast<Uint32>(vertices.size());
    mesh.index_count  = static_cast<Uint32>(indices.size());

    const SDL_GPUBufferCreateInfo vb_info{
        .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
        .size  = static_cast<Uint32>(vertices.size() * sizeof(VertexPosColor)),
        .props = 0,
    };
    mesh.vertex_buffer = SDL_CreateGPUBuffer(device, &vb_info);

    const SDL_GPUBufferCreateInfo ib_info{
        .usage = SDL_GPU_BUFFERUSAGE_INDEX,
        .size  = static_cast<Uint32>(indices.size() * sizeof(Uint16)),
        .props = 0,
    };
    mesh.index_buffer = SDL_CreateGPUBuffer(device, &ib_info);

    // Upload data
    const SDL_GPUTransferBufferCreateInfo tb_info{
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size  = vb_info.size + ib_info.size,
        .props = 0,
    };
    SDL_GPUTransferBuffer* transfer = SDL_CreateGPUTransferBuffer(device, &tb_info);

    void* ptr = SDL_MapGPUTransferBuffer(device, transfer, false);
    std::memcpy(ptr, vertices.data(), vb_info.size);
    std::memcpy(static_cast<char*>(ptr) + vb_info.size, indices.data(), ib_info.size);
    SDL_UnmapGPUTransferBuffer(device, transfer);

    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(device);
    SDL_GPUCopyPass*      copy = SDL_BeginGPUCopyPass(cmd);

    SDL_GPUTransferBufferLocation src_vb{ .transfer_buffer = transfer, .offset = 0 };
    SDL_GPUBufferRegion dst_vb{ .buffer = mesh.vertex_buffer, .offset = 0, .size = vb_info.size };
    SDL_UploadToGPUBuffer(copy, &src_vb, &dst_vb, false);

    SDL_GPUTransferBufferLocation src_ib{ .transfer_buffer = transfer, .offset = vb_info.size };
    SDL_GPUBufferRegion dst_ib{ .buffer = mesh.index_buffer, .offset = 0, .size = ib_info.size };
    SDL_UploadToGPUBuffer(copy, &src_ib, &dst_ib, false);

    SDL_EndGPUCopyPass(copy);
    SDL_SubmitGPUCommandBuffer(cmd);

    SDL_ReleaseGPUTransferBuffer(device, transfer);

    return mesh;
}

void destroy_mesh(SDL_GPUDevice* device, GPUMesh& mesh)
{
    if (mesh.vertex_buffer)
    {
        SDL_ReleaseGPUBuffer(device, mesh.vertex_buffer);
        mesh.vertex_buffer = nullptr;
    }
    if (mesh.index_buffer)
    {
        SDL_ReleaseGPUBuffer(device, mesh.index_buffer);
        mesh.index_buffer = nullptr;
    }
}

} // namespace as3
