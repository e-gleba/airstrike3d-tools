#include "render.hpp"
#include "shader.hpp"
#include "texture.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <ranges>

namespace euengine
{

namespace
{

/// Unit cube corner offsets
constexpr std::array<glm::vec3, 8> k_cube_offsets { {
    { -1, -1, -1 },
    { 1, -1, -1 },
    { 1, 1, -1 },
    { -1, 1, -1 },
    { -1, -1, 1 },
    { 1, -1, 1 },
    { 1, 1, 1 },
    { -1, 1, 1 },
} };

/// Cube edge pairs (vertex indices)
constexpr std::array<std::pair<std::size_t, std::size_t>, 12> k_cube_edges { {
    { 0, 1 },
    { 1, 2 },
    { 2, 3 },
    { 3, 0 },
    { 4, 5 },
    { 5, 6 },
    { 6, 7 },
    { 7, 4 },
    { 0, 4 },
    { 1, 5 },
    { 2, 6 },
    { 3, 7 },
} };

/// Texture file extensions to search for
constexpr std::array k_texture_extensions { ".tga", ".TGA", ".png", ".PNG",
                                            ".jpg", ".JPG", ".jpeg" };

/// Two PI for circle calculations
constexpr float k_two_pi = 6.28318530718f;

} // namespace

Renderer::~Renderer()
{
    shutdown();
}

bool Renderer::init(SDL_GPUDevice* device, ShaderManager* shaders)
{
    device_  = device;
    shaders_ = shaders;

    // Load wireframe shader program
    const ShaderProgramDesc wireframe_desc {
        .name     = "wireframe",
        .vertex   = { .path  = "wireframe.vert.hlsl",
                      .stage = ShaderStage::Vertex },
        .fragment = { .path  = "wireframe.frag.hlsl",
                      .stage = ShaderStage::Fragment },
    };
    if (auto result = shaders_->load_program(wireframe_desc); !result)
    {
        spdlog::error("=> load wireframe shader: {}", result.error());
        return false;
    }

    // Load textured shader program
    const ShaderProgramDesc textured_desc {
        .name     = "textured",
        .vertex   = { .path  = "textured.vert.hlsl",
                      .stage = ShaderStage::Vertex },
        .fragment = { .path  = "textured.frag.hlsl",
                      .stage = ShaderStage::Fragment },
    };
    if (auto result = shaders_->load_program(textured_desc); !result)
    {
        spdlog::error("=> load textured shader: {}", result.error());
        return false;
    }

    // Load post-processing shader program
    const ShaderProgramDesc postprocess_desc {
        .name     = "postprocess",
        .vertex   = { .path  = "postprocess.vert.hlsl",
                      .stage = ShaderStage::Vertex },
        .fragment = { .path  = "postprocess.frag.hlsl",
                      .stage = ShaderStage::Fragment },
    };
    if (auto result = shaders_->load_program(postprocess_desc); !result)
    {
        spdlog::error("=> load postprocess shader: {}", result.error());
        return false;
    }

    // Setup shader hot-reload callback
    shaders_->set_reload_callback(
        [this](const std::string& name)
        {
            if (name == "wireframe" || name == "textured" ||
                name == "postprocess")
            {
                pipeline_dirty_ = true;
            }
        });

    if (!create_wireframe_pipeline() || !create_textured_pipeline() ||
        !create_postprocess_pipeline())
    {
        return false;
    }

    // Create default white texture
    if (auto tex = create_default_texture(device_))
    {
        default_texture_            = next_texture_handle_++;
        textures_[default_texture_] = { .texture = tex->texture,
                                        .sampler = tex->sampler,
                                        .width   = tex->width,
                                        .height  = tex->height };
    }

    return true;
}

void Renderer::shutdown()
{
    // Release temporary debug meshes
    for (auto& mesh : temp_meshes_)
    {
        if (mesh.vertex_buffer != nullptr)
        {
            SDL_ReleaseGPUBuffer(device_, mesh.vertex_buffer);
        }
        if (mesh.index_buffer != nullptr)
        {
            SDL_ReleaseGPUBuffer(device_, mesh.index_buffer);
        }
    }
    temp_meshes_.clear();

    // Release all meshes
    for (auto& [handle, mesh] : meshes_)
    {
        if (mesh.vertex_buffer != nullptr)
        {
            SDL_ReleaseGPUBuffer(device_, mesh.vertex_buffer);
        }
        if (mesh.index_buffer != nullptr)
        {
            SDL_ReleaseGPUBuffer(device_, mesh.index_buffer);
        }
    }
    meshes_.clear();

    // Release all models
    for (auto& [handle, model] : models_)
    {
        for (auto& mesh : model.meshes)
        {
            if (mesh.vertex_buffer != nullptr)
            {
                SDL_ReleaseGPUBuffer(device_, mesh.vertex_buffer);
            }
            if (mesh.index_buffer != nullptr)
            {
                SDL_ReleaseGPUBuffer(device_, mesh.index_buffer);
            }
        }
    }
    models_.clear();

    // Release all textures
    for (auto& [handle, tex] : textures_)
    {
        if (tex.texture != nullptr)
        {
            SDL_ReleaseGPUTexture(device_, tex.texture);
        }
        if (tex.sampler != nullptr)
        {
            SDL_ReleaseGPUSampler(device_, tex.sampler);
        }
    }
    textures_.clear();

    // Release pipelines
    if (wireframe_pipeline_ != nullptr)
    {
        SDL_ReleaseGPUGraphicsPipeline(device_, wireframe_pipeline_);
        wireframe_pipeline_ = nullptr;
    }
    if (wireframe_tri_pipeline_ != nullptr)
    {
        SDL_ReleaseGPUGraphicsPipeline(device_, wireframe_tri_pipeline_);
        wireframe_tri_pipeline_ = nullptr;
    }
    if (textured_pipeline_ != nullptr)
    {
        SDL_ReleaseGPUGraphicsPipeline(device_, textured_pipeline_);
        textured_pipeline_ = nullptr;
    }
    if (textured_wireframe_pipeline_ != nullptr)
    {
        SDL_ReleaseGPUGraphicsPipeline(device_, textured_wireframe_pipeline_);
        textured_wireframe_pipeline_ = nullptr;
    }
    if (depth_texture_ != nullptr)
    {
        SDL_ReleaseGPUTexture(device_, depth_texture_);
        depth_texture_ = nullptr;
    }

    // Release MSAA render targets
    if (msaa_color_texture_ != nullptr)
    {
        SDL_ReleaseGPUTexture(device_, msaa_color_texture_);
        msaa_color_texture_ = nullptr;
    }
    if (msaa_depth_texture_ != nullptr)
    {
        SDL_ReleaseGPUTexture(device_, msaa_depth_texture_);
        msaa_depth_texture_ = nullptr;
    }
    if (msaa_resolve_texture_ != nullptr)
    {
        SDL_ReleaseGPUTexture(device_, msaa_resolve_texture_);
        msaa_resolve_texture_ = nullptr;
    }

    // Release post-processing resources
    if (pp_color_texture_ != nullptr)
    {
        SDL_ReleaseGPUTexture(device_, pp_color_texture_);
        pp_color_texture_ = nullptr;
    }
    if (pp_sampler_ != nullptr)
    {
        SDL_ReleaseGPUSampler(device_, pp_sampler_);
        pp_sampler_ = nullptr;
    }
    if (postprocess_pipeline_ != nullptr)
    {
        SDL_ReleaseGPUGraphicsPipeline(device_, postprocess_pipeline_);
        postprocess_pipeline_ = nullptr;
    }
}

void Renderer::ensure_depth_texture(Uint32 width, Uint32 height)
{
    if (width == 0 || height == 0)
    {
        return;
    }
    if ((depth_texture_ != nullptr) && depth_width_ == width &&
        depth_height_ == height)
    {
        return;
    }

    if (depth_texture_ != nullptr)
    {
        SDL_ReleaseGPUTexture(device_, depth_texture_);
        depth_texture_ = nullptr;
    }

    SDL_GPUTextureCreateInfo info {};
    info.type                 = SDL_GPU_TEXTURETYPE_2D;
    info.format               = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    info.usage                = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
    info.width                = width;
    info.height               = height;
    info.layer_count_or_depth = 1;
    info.num_levels           = 1;
    info.sample_count         = SDL_GPU_SAMPLECOUNT_1;

    depth_texture_ = SDL_CreateGPUTexture(device_, &info);
    if (depth_texture_ != nullptr)
    {
        depth_width_  = width;
        depth_height_ = height;
    }
    else
    {
        spdlog::error("== depth texture: {}", SDL_GetError());
    }
}

void Renderer::ensure_msaa_targets(Uint32               width,
                                   Uint32               height,
                                   SDL_GPUTextureFormat format)
{
    // Skip if MSAA is disabled
    if (msaa_samples_ == msaa_samples::none)
    {
        // Release existing MSAA textures if any
        if (msaa_color_texture_ != nullptr)
        {
            SDL_ReleaseGPUTexture(device_, msaa_color_texture_);
            msaa_color_texture_ = nullptr;
        }
        if (msaa_depth_texture_ != nullptr)
        {
            SDL_ReleaseGPUTexture(device_, msaa_depth_texture_);
            msaa_depth_texture_ = nullptr;
        }
        if (msaa_resolve_texture_ != nullptr)
        {
            SDL_ReleaseGPUTexture(device_, msaa_resolve_texture_);
            msaa_resolve_texture_ = nullptr;
        }
        msaa_width_  = 0;
        msaa_height_ = 0;
        return;
    }

    // Check if we need to recreate
    if (msaa_color_texture_ != nullptr && msaa_width_ == width &&
        msaa_height_ == height)
    {
        return; // Already have correct size
    }

    // Release old textures
    if (msaa_color_texture_ != nullptr)
    {
        SDL_ReleaseGPUTexture(device_, msaa_color_texture_);
        msaa_color_texture_ = nullptr;
    }
    if (msaa_depth_texture_ != nullptr)
    {
        SDL_ReleaseGPUTexture(device_, msaa_depth_texture_);
        msaa_depth_texture_ = nullptr;
    }
    if (msaa_resolve_texture_ != nullptr)
    {
        SDL_ReleaseGPUTexture(device_, msaa_resolve_texture_);
        msaa_resolve_texture_ = nullptr;
    }

    // Determine sample count
    SDL_GPUSampleCount sample_count = SDL_GPU_SAMPLECOUNT_1;
    switch (msaa_samples_)
    {
        case msaa_samples::none:
            sample_count = SDL_GPU_SAMPLECOUNT_1;
            break;
        case msaa_samples::x2:
            sample_count = SDL_GPU_SAMPLECOUNT_2;
            break;
        case msaa_samples::x4:
            sample_count = SDL_GPU_SAMPLECOUNT_4;
            break;
        case msaa_samples::x8:
            sample_count = SDL_GPU_SAMPLECOUNT_8;
            break;
        case msaa_samples::x16:
            sample_count = SDL_GPU_SAMPLECOUNT_8;
            break; // SDL_GPU max is 8x
    }

    // Check if this sample count is supported
    if (!SDL_GPUTextureSupportsSampleCount(device_, format, sample_count))
    {
        spdlog::warn(
            "MSAA {}x not supported for this format, falling back to 1x",
            static_cast<int>(sample_count));
        msaa_samples_   = msaa_samples::none;
        pipeline_dirty_ = true;
        return;
    }

    spdlog::info("Creating MSAA {}x render targets ({}x{})",
                 static_cast<int>(sample_count),
                 width,
                 height);

    // Create MSAA color texture
    SDL_GPUTextureCreateInfo color_info {};
    color_info.type                 = SDL_GPU_TEXTURETYPE_2D;
    color_info.format               = format;
    color_info.usage                = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
    color_info.width                = width;
    color_info.height               = height;
    color_info.layer_count_or_depth = 1;
    color_info.num_levels           = 1;
    color_info.sample_count         = sample_count;

    msaa_color_texture_ = SDL_CreateGPUTexture(device_, &color_info);
    if (msaa_color_texture_ == nullptr)
    {
        spdlog::error("Failed to create MSAA color texture: {}",
                      SDL_GetError());
        return;
    }

    // Create MSAA depth texture
    SDL_GPUTextureCreateInfo depth_info {};
    depth_info.type                 = SDL_GPU_TEXTURETYPE_2D;
    depth_info.format               = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    depth_info.usage                = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
    depth_info.width                = width;
    depth_info.height               = height;
    depth_info.layer_count_or_depth = 1;
    depth_info.num_levels           = 1;
    depth_info.sample_count         = sample_count;

    msaa_depth_texture_ = SDL_CreateGPUTexture(device_, &depth_info);
    if (msaa_depth_texture_ == nullptr)
    {
        spdlog::error("Failed to create MSAA depth texture: {}",
                      SDL_GetError());
        SDL_ReleaseGPUTexture(device_, msaa_color_texture_);
        msaa_color_texture_ = nullptr;
        return;
    }

    msaa_width_  = width;
    msaa_height_ = height;

    spdlog::info("MSAA render targets created successfully");
}

void Renderer::resolve_msaa(SDL_GPUCommandBuffer* cmd, SDL_GPUTexture* target)
{
    if (msaa_samples_ == msaa_samples::none || msaa_color_texture_ == nullptr)
    {
        return; // Nothing to resolve
    }

    // Blit from MSAA texture to swapchain (this resolves the MSAA)
    SDL_GPUBlitInfo blit_info {};
    blit_info.source.texture      = msaa_color_texture_;
    blit_info.source.w            = msaa_width_;
    blit_info.source.h            = msaa_height_;
    blit_info.destination.texture = target;
    blit_info.destination.w       = msaa_width_;
    blit_info.destination.h       = msaa_height_;
    blit_info.load_op             = SDL_GPU_LOADOP_DONT_CARE;
    blit_info.filter              = SDL_GPU_FILTER_LINEAR;

    SDL_BlitGPUTexture(cmd, &blit_info);
}

bool Renderer::create_wireframe_pipeline()
{
    auto* prog = shaders_->get_program("wireframe");
    if ((prog == nullptr) || !prog->valid())
    {
        return false;
    }

    std::array<SDL_GPUVertexAttribute, 2> attrs {};
    attrs[0].location    = 0;
    attrs[0].buffer_slot = 0;
    attrs[0].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    attrs[0].offset      = 0;
    attrs[1].location    = 1;
    attrs[1].buffer_slot = 0;
    attrs[1].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    attrs[1].offset      = sizeof(glm::vec3);

    SDL_GPUVertexBufferDescription vb_desc {};
    vb_desc.slot       = 0;
    vb_desc.pitch      = sizeof(vertex_pos_color);
    vb_desc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

    SDL_GPUVertexInputState vertex_input {};
    vertex_input.vertex_buffer_descriptions = &vb_desc;
    vertex_input.num_vertex_buffers         = 1;
    vertex_input.vertex_attributes          = attrs.data();
    vertex_input.num_vertex_attributes      = static_cast<Uint32>(attrs.size());

    SDL_GPUColorTargetDescription color_target {};
    color_target.format = SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
    color_target.blend_state.enable_blend = false;

    SDL_GPUGraphicsPipelineTargetInfo target_info {};
    target_info.color_target_descriptions = &color_target;
    target_info.num_color_targets         = 1;
    target_info.depth_stencil_format      = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    target_info.has_depth_stencil_target  = true;

    SDL_GPURasterizerState raster_state {};
    raster_state.fill_mode  = SDL_GPU_FILLMODE_FILL;
    raster_state.cull_mode  = SDL_GPU_CULLMODE_NONE;
    raster_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;

    SDL_GPUMultisampleState ms_state {};
    // Convert msaa_samples to SDL_GPUSampleCount
    SDL_GPUSampleCount sample_count = SDL_GPU_SAMPLECOUNT_1;
    switch (msaa_samples_)
    {
        case msaa_samples::none:
            sample_count = SDL_GPU_SAMPLECOUNT_1;
            break;
        case msaa_samples::x2:
            sample_count = SDL_GPU_SAMPLECOUNT_2;
            break;
        case msaa_samples::x4:
            sample_count = SDL_GPU_SAMPLECOUNT_4;
            break;
        case msaa_samples::x8:
            sample_count = SDL_GPU_SAMPLECOUNT_8;
            break;
        case msaa_samples::x16:
            // SDL_GPU may not support 16x, try 8x as fallback
            sample_count = SDL_GPU_SAMPLECOUNT_8;
            break;
    }
    ms_state.sample_count = sample_count;
    spdlog::debug("Creating pipeline with MSAA: {}x (sample_count={})",
                  static_cast<int>(msaa_samples_),
                  static_cast<int>(sample_count));

    SDL_GPUDepthStencilState depth_state {};
    depth_state.compare_op         = SDL_GPU_COMPAREOP_LESS;
    depth_state.enable_depth_test  = true;
    depth_state.enable_depth_write = true;

    SDL_GPUGraphicsPipelineCreateInfo pipeline_info {};
    pipeline_info.vertex_shader       = prog->vertex_shader();
    pipeline_info.fragment_shader     = prog->fragment_shader();
    pipeline_info.vertex_input_state  = vertex_input;
    pipeline_info.primitive_type      = SDL_GPU_PRIMITIVETYPE_LINELIST;
    pipeline_info.rasterizer_state    = raster_state;
    pipeline_info.multisample_state   = ms_state;
    pipeline_info.depth_stencil_state = depth_state;
    pipeline_info.target_info         = target_info;

    if (wireframe_pipeline_ != nullptr)
    {
        SDL_ReleaseGPUGraphicsPipeline(device_, wireframe_pipeline_);
    }

    wireframe_pipeline_ =
        SDL_CreateGPUGraphicsPipeline(device_, &pipeline_info);
    if (wireframe_pipeline_ == nullptr)
    {
        return false;
    }

    // Create triangle variant for thick lines
    pipeline_info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    if (wireframe_tri_pipeline_ != nullptr)
    {
        SDL_ReleaseGPUGraphicsPipeline(device_, wireframe_tri_pipeline_);
    }
    wireframe_tri_pipeline_ =
        SDL_CreateGPUGraphicsPipeline(device_, &pipeline_info);
    return wireframe_tri_pipeline_ != nullptr;
}

bool Renderer::create_textured_pipeline()
{
    auto* prog = shaders_->get_program("textured");
    if ((prog == nullptr) || !prog->valid())
    {
        return false;
    }

    std::array<SDL_GPUVertexAttribute, 3> attrs {};
    attrs[0].location    = 0;
    attrs[0].buffer_slot = 0;
    attrs[0].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    attrs[0].offset      = offsetof(vertex_textured, position);
    attrs[1].location    = 1;
    attrs[1].buffer_slot = 0;
    attrs[1].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    attrs[1].offset      = offsetof(vertex_textured, normal);
    attrs[2].location    = 2;
    attrs[2].buffer_slot = 0;
    attrs[2].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    attrs[2].offset      = offsetof(vertex_textured, texcoord);

    SDL_GPUVertexBufferDescription vb_desc {};
    vb_desc.slot       = 0;
    vb_desc.pitch      = sizeof(vertex_textured);
    vb_desc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

    SDL_GPUVertexInputState vertex_input {};
    vertex_input.vertex_buffer_descriptions = &vb_desc;
    vertex_input.num_vertex_buffers         = 1;
    vertex_input.vertex_attributes          = attrs.data();
    vertex_input.num_vertex_attributes      = static_cast<Uint32>(attrs.size());

    SDL_GPUColorTargetDescription color_target {};
    color_target.format = SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
    color_target.blend_state.enable_blend = false;

    SDL_GPUGraphicsPipelineTargetInfo target_info {};
    target_info.color_target_descriptions = &color_target;
    target_info.num_color_targets         = 1;
    target_info.depth_stencil_format      = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    target_info.has_depth_stencil_target  = true;

    SDL_GPURasterizerState raster_state {};
    raster_state.fill_mode  = SDL_GPU_FILLMODE_FILL;
    raster_state.cull_mode  = SDL_GPU_CULLMODE_BACK;
    raster_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;

    SDL_GPUMultisampleState ms_state {};
    // Convert msaa_samples to SDL_GPUSampleCount
    SDL_GPUSampleCount sample_count = SDL_GPU_SAMPLECOUNT_1;
    switch (msaa_samples_)
    {
        case msaa_samples::none:
            sample_count = SDL_GPU_SAMPLECOUNT_1;
            break;
        case msaa_samples::x2:
            sample_count = SDL_GPU_SAMPLECOUNT_2;
            break;
        case msaa_samples::x4:
            sample_count = SDL_GPU_SAMPLECOUNT_4;
            break;
        case msaa_samples::x8:
            sample_count = SDL_GPU_SAMPLECOUNT_8;
            break;
        case msaa_samples::x16:
            // SDL_GPU may not support 16x, try 8x as fallback
            sample_count = SDL_GPU_SAMPLECOUNT_8;
            break;
    }
    ms_state.sample_count = sample_count;
    spdlog::debug("Creating pipeline with MSAA: {}x (sample_count={})",
                  static_cast<int>(msaa_samples_),
                  static_cast<int>(sample_count));

    SDL_GPUDepthStencilState depth_state {};
    depth_state.compare_op         = SDL_GPU_COMPAREOP_LESS;
    depth_state.enable_depth_test  = true;
    depth_state.enable_depth_write = true;

    SDL_GPUGraphicsPipelineCreateInfo pipeline_info {};
    pipeline_info.vertex_shader       = prog->vertex_shader();
    pipeline_info.fragment_shader     = prog->fragment_shader();
    pipeline_info.vertex_input_state  = vertex_input;
    pipeline_info.primitive_type      = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    pipeline_info.rasterizer_state    = raster_state;
    pipeline_info.multisample_state   = ms_state;
    pipeline_info.depth_stencil_state = depth_state;
    pipeline_info.target_info         = target_info;

    if (textured_pipeline_ != nullptr)
    {
        SDL_ReleaseGPUGraphicsPipeline(device_, textured_pipeline_);
    }

    textured_pipeline_ = SDL_CreateGPUGraphicsPipeline(device_, &pipeline_info);
    if (textured_pipeline_ == nullptr)
    {
        return false;
    }

    // Create wireframe variant
    raster_state.fill_mode         = SDL_GPU_FILLMODE_LINE;
    raster_state.cull_mode         = SDL_GPU_CULLMODE_NONE;
    pipeline_info.rasterizer_state = raster_state;

    if (textured_wireframe_pipeline_ != nullptr)
    {
        SDL_ReleaseGPUGraphicsPipeline(device_, textured_wireframe_pipeline_);
    }

    textured_wireframe_pipeline_ =
        SDL_CreateGPUGraphicsPipeline(device_, &pipeline_info);
    return true;
}

bool Renderer::create_postprocess_pipeline()
{
    auto* prog = shaders_->get_program("postprocess");
    if ((prog == nullptr) || !prog->valid())
    {
        spdlog::warn("Postprocess shader not available");
        return false;
    }

    // No vertex input - we generate vertices from vertex ID
    SDL_GPUVertexInputState vertex_input {};
    vertex_input.vertex_buffer_descriptions = nullptr;
    vertex_input.num_vertex_buffers         = 0;
    vertex_input.vertex_attributes          = nullptr;
    vertex_input.num_vertex_attributes      = 0;

    SDL_GPUColorTargetDescription color_target {};
    color_target.format = SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
    color_target.blend_state.enable_blend = false;

    SDL_GPUGraphicsPipelineTargetInfo target_info {};
    target_info.color_target_descriptions = &color_target;
    target_info.num_color_targets         = 1;
    target_info.has_depth_stencil_target  = false;

    SDL_GPURasterizerState raster_state {};
    raster_state.fill_mode  = SDL_GPU_FILLMODE_FILL;
    raster_state.cull_mode  = SDL_GPU_CULLMODE_NONE;
    raster_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;

    SDL_GPUMultisampleState ms_state {};
    ms_state.sample_count = SDL_GPU_SAMPLECOUNT_1;

    SDL_GPUGraphicsPipelineCreateInfo pipeline_info {};
    pipeline_info.vertex_shader      = prog->vertex_shader();
    pipeline_info.fragment_shader    = prog->fragment_shader();
    pipeline_info.vertex_input_state = vertex_input;
    pipeline_info.primitive_type     = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    pipeline_info.rasterizer_state   = raster_state;
    pipeline_info.multisample_state  = ms_state;
    pipeline_info.target_info        = target_info;

    if (postprocess_pipeline_ != nullptr)
    {
        SDL_ReleaseGPUGraphicsPipeline(device_, postprocess_pipeline_);
    }

    postprocess_pipeline_ =
        SDL_CreateGPUGraphicsPipeline(device_, &pipeline_info);
    if (postprocess_pipeline_ == nullptr)
    {
        spdlog::error("Failed to create postprocess pipeline: {}",
                      SDL_GetError());
        return false;
    }

    spdlog::info("Post-processing pipeline created successfully");
    return true;
}

void Renderer::ensure_pp_target(Uint32               width,
                                Uint32               height,
                                SDL_GPUTextureFormat format)
{
    // Check if we need to recreate
    if (pp_color_texture_ != nullptr && pp_width_ == width &&
        pp_height_ == height)
    {
        return;
    }

    // Release old texture
    if (pp_color_texture_ != nullptr)
    {
        SDL_ReleaseGPUTexture(device_, pp_color_texture_);
        pp_color_texture_ = nullptr;
    }
    if (pp_sampler_ != nullptr)
    {
        SDL_ReleaseGPUSampler(device_, pp_sampler_);
        pp_sampler_ = nullptr;
    }

    // Create color texture for scene rendering
    SDL_GPUTextureCreateInfo color_info {};
    color_info.type   = SDL_GPU_TEXTURETYPE_2D;
    color_info.format = format;
    color_info.usage =
        SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    color_info.width                = width;
    color_info.height               = height;
    color_info.layer_count_or_depth = 1;
    color_info.num_levels           = 1;
    color_info.sample_count         = SDL_GPU_SAMPLECOUNT_1;

    pp_color_texture_ = SDL_CreateGPUTexture(device_, &color_info);
    if (pp_color_texture_ == nullptr)
    {
        spdlog::error("Failed to create post-process color texture: {}",
                      SDL_GetError());
        return;
    }

    // Create sampler for reading the texture in post-process pass
    SDL_GPUSamplerCreateInfo samp_info {};
    samp_info.min_filter     = SDL_GPU_FILTER_LINEAR;
    samp_info.mag_filter     = SDL_GPU_FILTER_LINEAR;
    samp_info.mipmap_mode    = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    samp_info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    samp_info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    samp_info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;

    pp_sampler_ = SDL_CreateGPUSampler(device_, &samp_info);

    pp_width_  = width;
    pp_height_ = height;

    spdlog::info(
        "Post-processing render target created ({}x{})", width, height);
}

void Renderer::apply_postprocess(SDL_GPUCommandBuffer*     cmd,
                                 SDL_GPUTexture*           target,
                                 const postprocess_params& params)
{
    if (postprocess_pipeline_ == nullptr || pp_color_texture_ == nullptr ||
        pp_sampler_ == nullptr)
    {
        return;
    }

    // Setup render pass to output to final target (swapchain)
    SDL_GPUColorTargetInfo color_target {};
    color_target.texture = target;
    color_target.load_op =
        SDL_GPU_LOADOP_DONT_CARE; // We'll overwrite everything
    color_target.store_op = SDL_GPU_STOREOP_STORE;

    auto* pass = SDL_BeginGPURenderPass(cmd, &color_target, 1, nullptr);
    if (pass == nullptr)
    {
        spdlog::error("Failed to begin postprocess render pass");
        return;
    }

    SDL_BindGPUGraphicsPipeline(pass, postprocess_pipeline_);

    // Bind the scene texture
    SDL_GPUTextureSamplerBinding tex_binding {};
    tex_binding.texture = pp_color_texture_;
    tex_binding.sampler = pp_sampler_;
    SDL_BindGPUFragmentSamplers(pass, 0, &tex_binding, 1);

    // Push post-process parameters
    SDL_PushGPUFragmentUniformData(cmd, 0, &params, sizeof(params));

    // Draw fullscreen triangle (3 vertices, generated in shader)
    SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);

    SDL_EndGPURenderPass(pass);
}

void Renderer::reload_pipelines()
{
    if (pipeline_dirty_)
    {
        (void)create_wireframe_pipeline(); // This also creates
                                           // wireframe_tri_pipeline_
        (void)create_textured_pipeline();
        (void)create_postprocess_pipeline();
        pipeline_dirty_ = false;
    }
}

void Renderer::begin_frame(SDL_GPUCommandBuffer* cmd, SDL_GPURenderPass* pass)
{
    current_cmd_  = cmd;
    current_pass_ = pass;

    // Clean up temporary debug meshes from previous frame
    for (auto& mesh : temp_meshes_)
    {
        if (mesh.vertex_buffer != nullptr)
        {
            SDL_ReleaseGPUBuffer(device_, mesh.vertex_buffer);
        }
        if (mesh.index_buffer != nullptr)
        {
            SDL_ReleaseGPUBuffer(device_, mesh.index_buffer);
        }
    }
    temp_meshes_.clear();

    // Reset per-frame stats
    frame_stats_ = render_stats {
        .models_loaded   = static_cast<std::uint32_t>(models_.size()),
        .textures_loaded = static_cast<std::uint32_t>(textures_.size()),
        .meshes_loaded   = static_cast<std::uint32_t>(meshes_.size()),
    };

    reload_pipelines();
}

void Renderer::end_frame()
{
    current_cmd_  = nullptr;
    current_pass_ = nullptr;
}

void Renderer::set_view_projection(const glm::mat4& vp)
{
    view_proj_ = vp;
}

void Renderer::set_render_mode(render_mode mode)
{
    render_mode_ = mode;
}

void Renderer::bind_pipeline()
{
    if ((wireframe_pipeline_ != nullptr) && (current_pass_ != nullptr))
    {
        SDL_BindGPUGraphicsPipeline(current_pass_, wireframe_pipeline_);
    }
}

gpu_mesh Renderer::upload_wireframe_mesh(
    std::span<const vertex_pos_color> verts, std::span<const uint16_t> idx)
{
    gpu_mesh mesh {};
    mesh.index_count  = static_cast<Uint32>(idx.size());
    mesh.vertex_count = static_cast<Uint32>(verts.size());

    const auto vb_size = static_cast<Uint32>(verts.size_bytes());
    const auto ib_size = static_cast<Uint32>(idx.size_bytes());

    // Create vertex buffer
    SDL_GPUBufferCreateInfo vb_info {};
    vb_info.usage      = SDL_GPU_BUFFERUSAGE_VERTEX;
    vb_info.size       = vb_size;
    mesh.vertex_buffer = SDL_CreateGPUBuffer(device_, &vb_info);

    // Create index buffer
    SDL_GPUBufferCreateInfo ib_info {};
    ib_info.usage     = SDL_GPU_BUFFERUSAGE_INDEX;
    ib_info.size      = ib_size;
    mesh.index_buffer = SDL_CreateGPUBuffer(device_, &ib_info);

    // Create transfer buffer and upload
    SDL_GPUTransferBufferCreateInfo tb_info {};
    tb_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tb_info.size  = vb_size + ib_size;
    auto* tb      = SDL_CreateGPUTransferBuffer(device_, &tb_info);
    auto* ptr     = SDL_MapGPUTransferBuffer(device_, tb, false);
    std::memcpy(ptr, verts.data(), vb_size);
    std::memcpy(static_cast<char*>(ptr) + vb_size, idx.data(), ib_size);
    SDL_UnmapGPUTransferBuffer(device_, tb);

    auto* cmd = SDL_AcquireGPUCommandBuffer(device_);
    auto* cp  = SDL_BeginGPUCopyPass(cmd);

    // Upload vertex data
    SDL_GPUTransferBufferLocation src1 {};
    src1.transfer_buffer = tb;
    src1.offset          = 0;
    SDL_GPUBufferRegion dst1 {};
    dst1.buffer = mesh.vertex_buffer;
    dst1.offset = 0;
    dst1.size   = vb_size;
    SDL_UploadToGPUBuffer(cp, &src1, &dst1, false);

    // Upload index data
    SDL_GPUTransferBufferLocation src2 {};
    src2.transfer_buffer = tb;
    src2.offset          = vb_size;
    SDL_GPUBufferRegion dst2 {};
    dst2.buffer = mesh.index_buffer;
    dst2.offset = 0;
    dst2.size   = ib_size;
    SDL_UploadToGPUBuffer(cp, &src2, &dst2, false);

    SDL_EndGPUCopyPass(cp);
    SDL_SubmitGPUCommandBuffer(cmd);
    SDL_ReleaseGPUTransferBuffer(device_, tb);

    return mesh;
}

gpu_textured_mesh Renderer::upload_textured_mesh(
    std::span<const vertex_textured> verts, std::span<const uint16_t> idx)
{
    gpu_textured_mesh mesh {};
    mesh.index_count  = static_cast<Uint32>(idx.size());
    mesh.vertex_count = static_cast<Uint32>(verts.size());

    const auto vb_size = static_cast<Uint32>(verts.size_bytes());
    const auto ib_size = static_cast<Uint32>(idx.size_bytes());

    // Create vertex buffer
    SDL_GPUBufferCreateInfo vb_info {};
    vb_info.usage      = SDL_GPU_BUFFERUSAGE_VERTEX;
    vb_info.size       = vb_size;
    mesh.vertex_buffer = SDL_CreateGPUBuffer(device_, &vb_info);

    // Create index buffer
    SDL_GPUBufferCreateInfo ib_info {};
    ib_info.usage     = SDL_GPU_BUFFERUSAGE_INDEX;
    ib_info.size      = ib_size;
    mesh.index_buffer = SDL_CreateGPUBuffer(device_, &ib_info);

    // Create transfer buffer and upload
    SDL_GPUTransferBufferCreateInfo tb_info {};
    tb_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tb_info.size  = vb_size + ib_size;
    auto* tb      = SDL_CreateGPUTransferBuffer(device_, &tb_info);
    auto* ptr     = SDL_MapGPUTransferBuffer(device_, tb, false);
    std::memcpy(ptr, verts.data(), vb_size);
    std::memcpy(static_cast<char*>(ptr) + vb_size, idx.data(), ib_size);
    SDL_UnmapGPUTransferBuffer(device_, tb);

    auto* cmd = SDL_AcquireGPUCommandBuffer(device_);
    auto* cp  = SDL_BeginGPUCopyPass(cmd);

    // Upload vertex data
    SDL_GPUTransferBufferLocation src1 {};
    src1.transfer_buffer = tb;
    src1.offset          = 0;
    SDL_GPUBufferRegion dst1 {};
    dst1.buffer = mesh.vertex_buffer;
    dst1.offset = 0;
    dst1.size   = vb_size;
    SDL_UploadToGPUBuffer(cp, &src1, &dst1, false);

    // Upload index data
    SDL_GPUTransferBufferLocation src2 {};
    src2.transfer_buffer = tb;
    src2.offset          = vb_size;
    SDL_GPUBufferRegion dst2 {};
    dst2.buffer = mesh.index_buffer;
    dst2.offset = 0;
    dst2.size   = ib_size;
    SDL_UploadToGPUBuffer(cp, &src2, &dst2, false);

    SDL_EndGPUCopyPass(cp);
    SDL_SubmitGPUCommandBuffer(cmd);
    SDL_ReleaseGPUTransferBuffer(device_, tb);

    return mesh;
}

mesh_handle Renderer::create_wireframe_cube(const glm::vec3& center,
                                            float            size,
                                            const glm::vec3& color)
{
    const float half = size * 0.5f;

    // Generate vertices from cube offsets
    std::vector<vertex_pos_color> verts;
    verts.reserve(k_cube_offsets.size());
    for (const auto& offset : k_cube_offsets)
    {
        verts.push_back({ center + offset * half, color });
    }

    // Generate indices from edge pairs
    std::vector<uint16_t> indices;
    indices.reserve(k_cube_edges.size() * 2);
    for (const auto& [i, j] : k_cube_edges)
    {
        indices.push_back(static_cast<uint16_t>(i));
        indices.push_back(static_cast<uint16_t>(j));
    }

    meshes_[next_mesh_handle_] = upload_wireframe_mesh(verts, indices);
    return next_mesh_handle_++;
}

mesh_handle Renderer::create_wireframe_sphere(const glm::vec3& center,
                                              float            radius,
                                              const glm::vec3& color,
                                              int              seg)
{
    std::vector<vertex_pos_color> verts;
    std::vector<uint16_t>         indices;

    // Generate three orthogonal circles
    auto add_circle = [&](int axis1, int axis2)
    {
        const auto base = static_cast<uint16_t>(verts.size());
        for (int i = 0; i <= seg; ++i)
        {
            const float angle =
                k_two_pi * static_cast<float>(i) / static_cast<float>(seg);
            glm::vec3 pos = center;
            pos[axis1] += radius * std::cos(angle);
            pos[axis2] += radius * std::sin(angle);
            verts.push_back({ pos, color });

            if (i > 0)
            {
                indices.push_back(static_cast<uint16_t>(base + i - 1));
                indices.push_back(static_cast<uint16_t>(base + i));
            }
        }
    };

    add_circle(0, 1); // XY plane
    add_circle(0, 2); // XZ plane
    add_circle(1, 2); // YZ plane

    meshes_[next_mesh_handle_] = upload_wireframe_mesh(verts, indices);
    return next_mesh_handle_++;
}

mesh_handle Renderer::create_wireframe_grid(float            size,
                                            int              div,
                                            const glm::vec3& color)
{
    std::vector<vertex_pos_color> verts;
    std::vector<uint16_t>         indices;

    const float half = size * 0.5f;
    const float step = size / static_cast<float>(div);

    for (int i = 0; i <= div; ++i)
    {
        const float t    = -half + (step * static_cast<float>(i));
        const auto  base = static_cast<uint16_t>(verts.size());

        // Line parallel to Z axis
        verts.push_back({ { t, 0, -half }, color });
        verts.push_back({ { t, 0, half }, color });
        // Line parallel to X axis
        verts.push_back({ { -half, 0, t }, color });
        verts.push_back({ { half, 0, t }, color });

        indices.push_back(base);
        indices.push_back(static_cast<uint16_t>(base + 1));
        indices.push_back(static_cast<uint16_t>(base + 2));
        indices.push_back(static_cast<uint16_t>(base + 3));
    }

    meshes_[next_mesh_handle_] = upload_wireframe_mesh(verts, indices);
    return next_mesh_handle_++;
}

mesh_handle Renderer::create_mesh(std::span<const vertex>   verts,
                                  std::span<const uint16_t> idx,
                                  primitive_type            type)
{
    if (verts.empty() || idx.empty())
    {
        return invalid_mesh;
    }

    // Convert vertex format
    std::vector<vertex_pos_color> converted;
    converted.reserve(verts.size());
    std::ranges::transform(verts,
                           std::back_inserter(converted),
                           [](const vertex& v)
                           {
                               return vertex_pos_color { .position = v.position,
                                                         .color    = v.color };
                           });

    auto mesh                  = upload_wireframe_mesh(converted, idx);
    mesh.type                  = type;
    meshes_[next_mesh_handle_] = mesh;
    return next_mesh_handle_++;
}

void Renderer::destroy_mesh(mesh_handle h)
{
    if (auto it = meshes_.find(h); it != meshes_.end())
    {
        if (it->second.vertex_buffer != nullptr)
        {
            SDL_ReleaseGPUBuffer(device_, it->second.vertex_buffer);
        }
        if (it->second.index_buffer != nullptr)
        {
            SDL_ReleaseGPUBuffer(device_, it->second.index_buffer);
        }
        meshes_.erase(it);
    }
}

void Renderer::draw_mesh_internal(const gpu_mesh& m)
{
    if ((current_pass_ == nullptr) || (current_cmd_ == nullptr))
    {
        return;
    }

    const uniform_mvp uniforms { view_proj_ };
    SDL_PushGPUVertexUniformData(current_cmd_, 0, &uniforms, sizeof(uniforms));

    SDL_GPUBufferBinding vb {};
    vb.buffer = m.vertex_buffer;
    vb.offset = 0;
    SDL_BindGPUVertexBuffers(current_pass_, 0, &vb, 1);

    SDL_GPUBufferBinding ib {};
    ib.buffer = m.index_buffer;
    ib.offset = 0;
    SDL_BindGPUIndexBuffer(current_pass_, &ib, SDL_GPU_INDEXELEMENTSIZE_16BIT);
    SDL_DrawGPUIndexedPrimitives(current_pass_, m.index_count, 1, 0, 0, 0);

    ++frame_stats_.draw_calls;
    frame_stats_.vertices += m.vertex_count;
}

void Renderer::draw(mesh_handle h)
{
    if (auto it = meshes_.find(h); it != meshes_.end())
    {
        if (current_pass_ != nullptr)
        {
            // Select pipeline based on primitive type
            SDL_GPUGraphicsPipeline* pipeline = nullptr;
            if (it->second.type == primitive_type::triangles)
            {
                pipeline = wireframe_tri_pipeline_;
            }
            else
            {
                pipeline = wireframe_pipeline_;
            }

            if (pipeline != nullptr)
            {
                SDL_BindGPUGraphicsPipeline(current_pass_, pipeline);
            }
        }
        draw_mesh_internal(it->second);
    }
}

std::filesystem::path Renderer::find_texture_for_model(
    const std::filesystem::path& model_path)
{
    const auto dir  = model_path.parent_path();
    const auto stem = model_path.stem();

    // First try matching texture with same name
    for (const auto& ext : k_texture_extensions)
    {
        auto tex_path = dir / (stem.string() + ext);
        if (std::filesystem::exists(tex_path))
        {
            return tex_path;
        }
    }

    // Fall back to first texture file in directory
    for (const auto& entry : std::filesystem::directory_iterator(dir))
    {
        if (!entry.is_regular_file())
        {
            continue;
        }

        const auto ext        = entry.path().extension().string();
        const bool is_texture = std::ranges::any_of(
            k_texture_extensions, [&ext](const char* e) { return ext == e; });
        if (is_texture)
        {
            return entry.path();
        }
    }

    return {};
}

texture_handle Renderer::load_texture(const std::filesystem::path& path)
{
    auto result = euengine::load_texture(device_, path, true);
    if (!result)
    {
        spdlog::error("== texture {}: {}", path.string(), result.error());
        return invalid_texture;
    }

    textures_[next_texture_handle_] = { .texture = result->texture,
                                        .sampler = result->sampler,
                                        .width   = result->width,
                                        .height  = result->height };

    spdlog::info("=> texture: {} ({}x{})",
                 path.filename().string(),
                 result->width,
                 result->height);
    return next_texture_handle_++;
}

void Renderer::unload_texture(texture_handle h)
{
    if (h == default_texture_)
    {
        return;
    }

    if (auto it = textures_.find(h); it != textures_.end())
    {
        if (it->second.texture != nullptr)
        {
            SDL_ReleaseGPUTexture(device_, it->second.texture);
        }
        if (it->second.sampler != nullptr)
        {
            SDL_ReleaseGPUSampler(device_, it->second.sampler);
        }
        textures_.erase(it);
    }
}

gpu_model Renderer::upload_loaded_model(const loaded_model& data,
                                        const glm::vec3&    color)
{
    gpu_model model {};
    model.color            = color;
    model.has_uvs          = data.has_uvs;
    model.model_bounds.min = data.bounds.min;
    model.model_bounds.max = data.bounds.max;

    // Upload each mesh to GPU
    for (const auto& src_mesh : data.meshes)
    {
        // Convert model_vertex to vertex_textured
        std::vector<vertex_textured> verts;
        verts.reserve(src_mesh.vertices.size());
        for (const auto& v : src_mesh.vertices)
        {
            verts.push_back(vertex_textured {
                .position = v.position,
                .normal   = v.normal,
                .texcoord = v.texcoord,
            });
        }

        if (!verts.empty() && !src_mesh.indices.empty())
        {
            model.meshes.push_back(
                upload_textured_mesh(verts, src_mesh.indices));
        }
    }

    return model;
}

model_handle Renderer::load_model(const std::filesystem::path& path,
                                  const glm::vec3&             color)
{
    // Use modular loader registry
    auto result = get_model_loader_registry().load(path);
    if (!result)
    {
        spdlog::error("== model {}: {}", path.string(), result.error());
        return invalid_model;
    }

    auto& data = result.value();

    // Load texture - first from model data, then try to find by name
    texture_handle tex = invalid_texture;
    if (!data.texture_path.empty())
    {
        tex = load_texture(data.texture_path);
    }
    if (tex == invalid_texture)
    {
        if (auto tex_path = find_texture_for_model(path); !tex_path.empty())
        {
            tex = load_texture(tex_path);
        }
    }

    // Upload to GPU
    gpu_model model = upload_loaded_model(data, color);
    model.texture   = tex;

    if (model.meshes.empty())
    {
        spdlog::error("== model {}: no meshes uploaded", path.string());
        return invalid_model;
    }

    const auto h = next_model_handle_++;
    models_[h]   = std::move(model);

    // Determine loader type for logging
    auto ext = path.extension().string();
    std::ranges::transform(ext, ext.begin(), ::tolower);
    const char* type = (ext == ".gltf" || ext == ".glb") ? "gltf" : "obj";

    spdlog::info("=> model ({}): {} ({} meshes, {} verts)",
                 type,
                 path.filename().string(),
                 models_[h].meshes.size(),
                 data.total_vertices());
    return h;
}

void Renderer::unload_model(model_handle h)
{
    if (auto it = models_.find(h); it != models_.end())
    {
        for (auto& m : it->second.meshes)
        {
            if (m.vertex_buffer != nullptr)
            {
                SDL_ReleaseGPUBuffer(device_, m.vertex_buffer);
            }
            if (m.index_buffer != nullptr)
            {
                SDL_ReleaseGPUBuffer(device_, m.index_buffer);
            }
        }
        if (it->second.texture != invalid_texture)
        {
            unload_texture(it->second.texture);
        }
        models_.erase(it);
    }
}

void Renderer::draw_textured_mesh_internal(const gpu_textured_mesh& m,
                                           texture_handle           tex_handle)
{
    if ((current_pass_ == nullptr) || (current_cmd_ == nullptr))
    {
        return;
    }

    auto tex_it = textures_.find(tex_handle);
    if (tex_it == textures_.end())
    {
        tex_it = textures_.find(default_texture_);
    }
    if (tex_it == textures_.end())
    {
        return;
    }

    SDL_GPUTextureSamplerBinding tsb {};
    tsb.texture = tex_it->second.texture;
    tsb.sampler = tex_it->second.sampler;
    SDL_BindGPUFragmentSamplers(current_pass_, 0, &tsb, 1);

    SDL_GPUBufferBinding vb {};
    vb.buffer = m.vertex_buffer;
    vb.offset = 0;
    SDL_BindGPUVertexBuffers(current_pass_, 0, &vb, 1);

    SDL_GPUBufferBinding ib {};
    ib.buffer = m.index_buffer;
    ib.offset = 0;
    SDL_BindGPUIndexBuffer(current_pass_, &ib, SDL_GPU_INDEXELEMENTSIZE_16BIT);
    SDL_DrawGPUIndexedPrimitives(current_pass_, m.index_count, 1, 0, 0, 0);

    ++frame_stats_.draw_calls;
    frame_stats_.triangles += m.index_count / 3;
    frame_stats_.vertices += m.vertex_count;
}

void Renderer::draw_model(model_handle h, const transform& xform)
{
    auto it = models_.find(h);
    if (it == models_.end())
    {
        return;
    }

    const auto& model = it->second;

    // Build model matrix: translate -> rotate (YXZ order) -> scale
    // Note: Add 180 degree rotation to fix model front/back orientation
    constexpr float k_model_rotation_fix = 180.0f;

    auto model_mat = glm::mat4(1.0f);
    model_mat      = glm::translate(model_mat, xform.position);
    model_mat =
        glm::rotate(model_mat,
                    glm::radians(xform.rotation.y + k_model_rotation_fix),
                    glm::vec3(0, 1, 0));
    model_mat = glm::rotate(
        model_mat, glm::radians(xform.rotation.x), glm::vec3(1, 0, 0));
    model_mat = glm::rotate(
        model_mat, glm::radians(xform.rotation.z), glm::vec3(0, 0, 1));
    model_mat = glm::scale(model_mat, xform.scale);

    // Select pipeline based on render mode
    auto* pipeline = (render_mode_ == render_mode::wireframe)
                         ? textured_wireframe_pipeline_
                         : textured_pipeline_;
    if ((pipeline != nullptr) && (current_pass_ != nullptr))
    {
        SDL_BindGPUGraphicsPipeline(current_pass_, pipeline);
    }

    const uniform_mvp uniforms { view_proj_ * model_mat };
    SDL_PushGPUVertexUniformData(current_cmd_, 0, &uniforms, sizeof(uniforms));

    const auto tex =
        model.texture != invalid_texture ? model.texture : default_texture_;
    for (const auto& mesh : model.meshes)
    {
        draw_textured_mesh_internal(mesh, tex);
    }
}

bounds Renderer::get_bounds(model_handle h) const
{
    if (auto it = models_.find(h); it != models_.end())
    {
        return it->second.model_bounds;
    }
    return {};
}

void Renderer::draw_bounds(const bounds&    b,
                           const transform& xform,
                           const glm::vec3& color)
{
    if ((current_pass_ == nullptr) || (current_cmd_ == nullptr))
    {
        return;
    }

    // Build corners of the AABB in local space
    const glm::vec3 corners[8] = {
        { b.min.x, b.min.y, b.min.z }, { b.max.x, b.min.y, b.min.z },
        { b.max.x, b.max.y, b.min.z }, { b.min.x, b.max.y, b.min.z },
        { b.min.x, b.min.y, b.max.z }, { b.max.x, b.min.y, b.max.z },
        { b.max.x, b.max.y, b.max.z }, { b.min.x, b.max.y, b.max.z },
    };

    // Build model matrix
    auto model_mat = glm::mat4(1.0f);
    model_mat      = glm::translate(model_mat, xform.position);
    model_mat      = glm::rotate(
        model_mat, glm::radians(xform.rotation.y), glm::vec3(0, 1, 0));
    model_mat = glm::rotate(
        model_mat, glm::radians(xform.rotation.x), glm::vec3(1, 0, 0));
    model_mat = glm::rotate(
        model_mat, glm::radians(xform.rotation.z), glm::vec3(0, 0, 1));
    model_mat = glm::scale(model_mat, xform.scale);

    // Transform corners to world space
    std::vector<vertex_pos_color> verts;
    verts.reserve(8);
    for (const auto& c : corners)
    {
        const glm::vec4 world = model_mat * glm::vec4(c, 1.0f);
        verts.push_back({ glm::vec3(world), color });
    }

    // Edges of the box
    constexpr uint16_t edges[] = {
        0, 1, 1, 2, 2, 3, 3, 0, // Bottom face
        4, 5, 5, 6, 6, 7, 7, 4, // Top face
        0, 4, 1, 5, 2, 6, 3, 7, // Vertical edges
    };

    // Create mesh and store for cleanup at start of next frame
    auto mesh =
        upload_wireframe_mesh(verts, std::span<const uint16_t>(edges, 24));

    // Bind wireframe pipeline and draw
    if (wireframe_pipeline_ != nullptr)
    {
        SDL_BindGPUGraphicsPipeline(current_pass_, wireframe_pipeline_);
    }

    const uniform_mvp uniforms { view_proj_ };
    SDL_PushGPUVertexUniformData(current_cmd_, 0, &uniforms, sizeof(uniforms));

    draw_mesh_internal(mesh);

    // Store in temporary list - will be cleaned up at start of next frame
    temp_meshes_.push_back(mesh);
}

render_stats Renderer::get_stats() const noexcept
{
    return frame_stats_;
}

void Renderer::set_msaa_samples(msaa_samples samples)
{
    if (msaa_samples_ != samples)
    {
        msaa_samples_   = samples;
        pipeline_dirty_ = true; // Mark pipelines for recreation
        if (samples != msaa_samples::none)
        {
            spdlog::warn("MSAA set to {}x, but MSAA requires render target "
                         "implementation. "
                         "Currently only pipeline MSAA is configured - visual "
                         "effect may be limited. "
                         "Full MSAA requires creating MSAA render target and "
                         "resolving to swapchain.",
                         static_cast<int>(samples));
        }
        else
        {
            spdlog::info("MSAA disabled");
        }
    }
}

void Renderer::set_max_anisotropy(float anisotropy)
{
    if (max_anisotropy_ != anisotropy)
    {
        max_anisotropy_ = anisotropy;
        sampler_dirty_  = true; // Mark samplers for recreation
        spdlog::info("Max anisotropy set to {:.1f}, samplers will be recreated",
                     anisotropy);
        // Note: Full implementation would recreate all texture samplers
        // For now, this just stores the value for future texture loads
    }
}

void Renderer::set_texture_filter(texture_filter filter)
{
    texture_filter_ = filter;
    sampler_dirty_  = true;
    spdlog::info("Texture filter set to: {}",
                 filter == texture_filter::nearest  ? "Nearest"
                 : filter == texture_filter::linear ? "Linear"
                                                    : "Trilinear");
}

} // namespace euengine
