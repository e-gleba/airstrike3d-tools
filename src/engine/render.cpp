#include "render.hpp"
#include "shader.hpp"
#include "texture.hpp"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <glm/gtc/matrix_transform.hpp>
#include <spdlog/spdlog.h>

#include <array>
#include <cstring>
#include <limits>

namespace as3
{

namespace
{
constexpr std::array<glm::vec3, 8> k_cube_offsets = {{
    {-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1},
    {-1, -1,  1}, {1, -1,  1}, {1, 1,  1}, {-1, 1,  1},
}};

constexpr std::array<std::pair<size_t, size_t>, 12> k_cube_edges = {{
    {0,1}, {1,2}, {2,3}, {3,0}, {4,5}, {5,6}, {6,7}, {7,4}, {0,4}, {1,5}, {2,6}, {3,7},
}};
} // namespace

Renderer::~Renderer() { shutdown(); }

bool Renderer::init(SDL_GPUDevice* device, ShaderManager* shaders)
{
    device_  = device;
    shaders_ = shaders;

    const ShaderProgramDesc wireframe_desc{
        .name     = "wireframe",
        .vertex   = { .path = "wireframe.vert.hlsl", .stage = ShaderStage::Vertex },
        .fragment = { .path = "wireframe.frag.hlsl", .stage = ShaderStage::Fragment },
    };
    if (auto r = shaders_->load_program(wireframe_desc); !r)
    {
        spdlog::error("=> load wireframe shader: {}", r.error());
        return false;
    }

    const ShaderProgramDesc textured_desc{
        .name     = "textured",
        .vertex   = { .path = "textured.vert.hlsl", .stage = ShaderStage::Vertex },
        .fragment = { .path = "textured.frag.hlsl", .stage = ShaderStage::Fragment },
    };
    if (auto r = shaders_->load_program(textured_desc); !r)
    {
        spdlog::error("=> load textured shader: {}", r.error());
        return false;
    }

    shaders_->set_reload_callback([this](const std::string& n) {
        if (n == "wireframe" || n == "textured") pipeline_dirty_ = true;
    });

    if (!create_wireframe_pipeline() || !create_textured_pipeline()) return false;
    
    if (auto tex = create_default_texture(device_))
    {
        default_texture_ = next_texture_handle_++;
        textures_[default_texture_] = { tex->texture, tex->sampler, tex->width, tex->height };
    }

    return true;
}

void Renderer::shutdown()
{
    for (auto& [h, m] : meshes_)
    {
        if (m.vertex_buffer) SDL_ReleaseGPUBuffer(device_, m.vertex_buffer);
        if (m.index_buffer)  SDL_ReleaseGPUBuffer(device_, m.index_buffer);
    }
    meshes_.clear();

    for (auto& [h, model] : models_)
    {
        for (auto& m : model.meshes)
        {
            if (m.vertex_buffer) SDL_ReleaseGPUBuffer(device_, m.vertex_buffer);
            if (m.index_buffer)  SDL_ReleaseGPUBuffer(device_, m.index_buffer);
        }
    }
    models_.clear();
    
    for (auto& [h, tex] : textures_)
    {
        if (tex.texture) SDL_ReleaseGPUTexture(device_, tex.texture);
        if (tex.sampler) SDL_ReleaseGPUSampler(device_, tex.sampler);
    }
    textures_.clear();

    if (wireframe_pipeline_)          { SDL_ReleaseGPUGraphicsPipeline(device_, wireframe_pipeline_); wireframe_pipeline_ = nullptr; }
    if (textured_pipeline_)           { SDL_ReleaseGPUGraphicsPipeline(device_, textured_pipeline_); textured_pipeline_ = nullptr; }
    if (textured_wireframe_pipeline_) { SDL_ReleaseGPUGraphicsPipeline(device_, textured_wireframe_pipeline_); textured_wireframe_pipeline_ = nullptr; }
    if (depth_texture_)               { SDL_ReleaseGPUTexture(device_, depth_texture_); depth_texture_ = nullptr; }
}

void Renderer::ensure_depth_texture(Uint32 width, Uint32 height)
{
    if (width == 0 || height == 0) return;
    if (depth_texture_ && depth_width_ == width && depth_height_ == height) return;
    
    if (depth_texture_) { SDL_ReleaseGPUTexture(device_, depth_texture_); depth_texture_ = nullptr; }
    
    SDL_GPUTextureCreateInfo info{};
    info.type                 = SDL_GPU_TEXTURETYPE_2D;
    info.format               = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    info.usage                = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
    info.width                = width;
    info.height               = height;
    info.layer_count_or_depth = 1;
    info.num_levels           = 1;
    info.sample_count         = SDL_GPU_SAMPLECOUNT_1;
    
    depth_texture_ = SDL_CreateGPUTexture(device_, &info);
    if (depth_texture_) { depth_width_ = width; depth_height_ = height; }
    else                { spdlog::error("== depth texture: {}", SDL_GetError()); }
}

bool Renderer::create_wireframe_pipeline()
{
    auto* prog = shaders_->get_program("wireframe");
    if (!prog || !prog->valid()) return false;

    SDL_GPUVertexAttribute attrs[2] = {};
    attrs[0].location = 0;
    attrs[0].buffer_slot = 0;
    attrs[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    attrs[0].offset = 0;
    attrs[1].location = 1;
    attrs[1].buffer_slot = 0;
    attrs[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    attrs[1].offset = sizeof(glm::vec3);

    SDL_GPUVertexBufferDescription vb{};
    vb.slot = 0;
    vb.pitch = sizeof(vertex_pos_color);
    vb.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

    SDL_GPUVertexInputState vi{};
    vi.vertex_buffer_descriptions = &vb;
    vi.num_vertex_buffers = 1;
    vi.vertex_attributes = attrs;
    vi.num_vertex_attributes = 2;

    SDL_GPUColorTargetDescription ct{};
    ct.format = SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
    ct.blend_state.enable_blend = false;

    SDL_GPUGraphicsPipelineTargetInfo ti{};
    ti.color_target_descriptions = &ct;
    ti.num_color_targets = 1;
    ti.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    ti.has_depth_stencil_target = true;

    SDL_GPURasterizerState rs{};
    rs.fill_mode = SDL_GPU_FILLMODE_FILL;
    rs.cull_mode = SDL_GPU_CULLMODE_NONE;
    rs.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;

    SDL_GPUMultisampleState ms{};
    ms.sample_count = SDL_GPU_SAMPLECOUNT_1;

    SDL_GPUDepthStencilState ds{};
    ds.compare_op = SDL_GPU_COMPAREOP_LESS;
    ds.enable_depth_test = true;
    ds.enable_depth_write = true;

    SDL_GPUGraphicsPipelineCreateInfo info{};
    info.vertex_shader       = prog->vertex_shader();
    info.fragment_shader     = prog->fragment_shader();
    info.vertex_input_state  = vi;
    info.primitive_type      = SDL_GPU_PRIMITIVETYPE_LINELIST;
    info.rasterizer_state    = rs;
    info.multisample_state   = ms;
    info.depth_stencil_state = ds;
    info.target_info         = ti;

    if (wireframe_pipeline_) SDL_ReleaseGPUGraphicsPipeline(device_, wireframe_pipeline_);
    wireframe_pipeline_ = SDL_CreateGPUGraphicsPipeline(device_, &info);
    return wireframe_pipeline_ != nullptr;
}

bool Renderer::create_textured_pipeline()
{
    auto* prog = shaders_->get_program("textured");
    if (!prog || !prog->valid()) return false;

    SDL_GPUVertexAttribute attrs[3] = {};
    attrs[0].location = 0;
    attrs[0].buffer_slot = 0;
    attrs[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    attrs[0].offset = offsetof(vertex_textured, position);
    attrs[1].location = 1;
    attrs[1].buffer_slot = 0;
    attrs[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    attrs[1].offset = offsetof(vertex_textured, normal);
    attrs[2].location = 2;
    attrs[2].buffer_slot = 0;
    attrs[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    attrs[2].offset = offsetof(vertex_textured, texcoord);

    SDL_GPUVertexBufferDescription vb{};
    vb.slot = 0;
    vb.pitch = sizeof(vertex_textured);
    vb.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

    SDL_GPUVertexInputState vi{};
    vi.vertex_buffer_descriptions = &vb;
    vi.num_vertex_buffers = 1;
    vi.vertex_attributes = attrs;
    vi.num_vertex_attributes = 3;

    SDL_GPUColorTargetDescription ct{};
    ct.format = SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
    ct.blend_state.enable_blend = false;

    SDL_GPUGraphicsPipelineTargetInfo ti{};
    ti.color_target_descriptions = &ct;
    ti.num_color_targets = 1;
    ti.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    ti.has_depth_stencil_target = true;

    SDL_GPURasterizerState rs{};
    rs.fill_mode = SDL_GPU_FILLMODE_FILL;
    rs.cull_mode = SDL_GPU_CULLMODE_BACK;
    rs.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;

    SDL_GPUMultisampleState ms{};
    ms.sample_count = SDL_GPU_SAMPLECOUNT_1;

    SDL_GPUDepthStencilState ds{};
    ds.compare_op = SDL_GPU_COMPAREOP_LESS;
    ds.enable_depth_test = true;
    ds.enable_depth_write = true;

    SDL_GPUGraphicsPipelineCreateInfo info{};
    info.vertex_shader       = prog->vertex_shader();
    info.fragment_shader     = prog->fragment_shader();
    info.vertex_input_state  = vi;
    info.primitive_type      = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    info.rasterizer_state    = rs;
    info.multisample_state   = ms;
    info.depth_stencil_state = ds;
    info.target_info         = ti;

    if (textured_pipeline_) SDL_ReleaseGPUGraphicsPipeline(device_, textured_pipeline_);
    textured_pipeline_ = SDL_CreateGPUGraphicsPipeline(device_, &info);
    if (!textured_pipeline_) return false;
    
    rs.fill_mode = SDL_GPU_FILLMODE_LINE;
    rs.cull_mode = SDL_GPU_CULLMODE_NONE;
    info.rasterizer_state = rs;
    
    if (textured_wireframe_pipeline_) SDL_ReleaseGPUGraphicsPipeline(device_, textured_wireframe_pipeline_);
    textured_wireframe_pipeline_ = SDL_CreateGPUGraphicsPipeline(device_, &info);
    
    return true;
}

void Renderer::reload_pipelines()
{
    if (pipeline_dirty_)
    {
        create_wireframe_pipeline();
        create_textured_pipeline();
        pipeline_dirty_ = false;
    }
}

void Renderer::begin_frame(SDL_GPUCommandBuffer* cmd, SDL_GPURenderPass* pass)
{
    current_cmd_ = cmd;
    current_pass_ = pass;
    reload_pipelines();
}

void Renderer::end_frame()
{
    current_cmd_ = nullptr;
    current_pass_ = nullptr;
}

void Renderer::set_view_projection(const glm::mat4& vp) { view_proj_ = vp; }
void Renderer::set_render_mode(render_mode mode) { render_mode_ = mode; }

void Renderer::bind_pipeline()
{
    if (wireframe_pipeline_ && current_pass_)
        SDL_BindGPUGraphicsPipeline(current_pass_, wireframe_pipeline_);
}

gpu_mesh Renderer::upload_wireframe_mesh(std::span<const vertex_pos_color> verts, std::span<const uint16_t> idx)
{
    gpu_mesh mesh{};
    mesh.vertex_count = static_cast<Uint32>(verts.size());
    mesh.index_count  = static_cast<Uint32>(idx.size());

    const auto vb_size = static_cast<Uint32>(verts.size_bytes());
    const auto ib_size = static_cast<Uint32>(idx.size_bytes());

    SDL_GPUBufferCreateInfo vb_info{};
    vb_info.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    vb_info.size = vb_size;
    mesh.vertex_buffer = SDL_CreateGPUBuffer(device_, &vb_info);
    
    SDL_GPUBufferCreateInfo ib_info{};
    ib_info.usage = SDL_GPU_BUFFERUSAGE_INDEX;
    ib_info.size = ib_size;
    mesh.index_buffer = SDL_CreateGPUBuffer(device_, &ib_info);

    SDL_GPUTransferBufferCreateInfo tb_info{};
    tb_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tb_info.size = vb_size + ib_size;
    auto* tb = SDL_CreateGPUTransferBuffer(device_, &tb_info);
    auto* ptr = SDL_MapGPUTransferBuffer(device_, tb, false);
    std::memcpy(ptr, verts.data(), vb_size);
    std::memcpy(static_cast<char*>(ptr) + vb_size, idx.data(), ib_size);
    SDL_UnmapGPUTransferBuffer(device_, tb);

    auto* cmd = SDL_AcquireGPUCommandBuffer(device_);
    auto* cp = SDL_BeginGPUCopyPass(cmd);
    
    SDL_GPUTransferBufferLocation src1{};
    src1.transfer_buffer = tb;
    src1.offset = 0;
    SDL_GPUBufferRegion dst1{};
    dst1.buffer = mesh.vertex_buffer;
    dst1.offset = 0;
    dst1.size = vb_size;
    SDL_UploadToGPUBuffer(cp, &src1, &dst1, false);
    
    SDL_GPUTransferBufferLocation src2{};
    src2.transfer_buffer = tb;
    src2.offset = vb_size;
    SDL_GPUBufferRegion dst2{};
    dst2.buffer = mesh.index_buffer;
    dst2.offset = 0;
    dst2.size = ib_size;
    SDL_UploadToGPUBuffer(cp, &src2, &dst2, false);
    
    SDL_EndGPUCopyPass(cp);
    SDL_SubmitGPUCommandBuffer(cmd);
    SDL_ReleaseGPUTransferBuffer(device_, tb);

    return mesh;
}

gpu_textured_mesh Renderer::upload_textured_mesh(std::span<const vertex_textured> verts, std::span<const uint16_t> idx)
{
    gpu_textured_mesh mesh{};
    mesh.vertex_count = static_cast<Uint32>(verts.size());
    mesh.index_count  = static_cast<Uint32>(idx.size());

    const auto vb_size = static_cast<Uint32>(verts.size_bytes());
    const auto ib_size = static_cast<Uint32>(idx.size_bytes());

    SDL_GPUBufferCreateInfo vb_info{};
    vb_info.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    vb_info.size = vb_size;
    mesh.vertex_buffer = SDL_CreateGPUBuffer(device_, &vb_info);
    
    SDL_GPUBufferCreateInfo ib_info{};
    ib_info.usage = SDL_GPU_BUFFERUSAGE_INDEX;
    ib_info.size = ib_size;
    mesh.index_buffer = SDL_CreateGPUBuffer(device_, &ib_info);

    SDL_GPUTransferBufferCreateInfo tb_info{};
    tb_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tb_info.size = vb_size + ib_size;
    auto* tb = SDL_CreateGPUTransferBuffer(device_, &tb_info);
    auto* ptr = SDL_MapGPUTransferBuffer(device_, tb, false);
    std::memcpy(ptr, verts.data(), vb_size);
    std::memcpy(static_cast<char*>(ptr) + vb_size, idx.data(), ib_size);
    SDL_UnmapGPUTransferBuffer(device_, tb);

    auto* cmd = SDL_AcquireGPUCommandBuffer(device_);
    auto* cp = SDL_BeginGPUCopyPass(cmd);
    
    SDL_GPUTransferBufferLocation src1{};
    src1.transfer_buffer = tb;
    src1.offset = 0;
    SDL_GPUBufferRegion dst1{};
    dst1.buffer = mesh.vertex_buffer;
    dst1.offset = 0;
    dst1.size = vb_size;
    SDL_UploadToGPUBuffer(cp, &src1, &dst1, false);
    
    SDL_GPUTransferBufferLocation src2{};
    src2.transfer_buffer = tb;
    src2.offset = vb_size;
    SDL_GPUBufferRegion dst2{};
    dst2.buffer = mesh.index_buffer;
    dst2.offset = 0;
    dst2.size = ib_size;
    SDL_UploadToGPUBuffer(cp, &src2, &dst2, false);
    
    SDL_EndGPUCopyPass(cp);
    SDL_SubmitGPUCommandBuffer(cmd);
    SDL_ReleaseGPUTransferBuffer(device_, tb);

    return mesh;
}

mesh_handle Renderer::create_wireframe_cube(const glm::vec3& center, float size, const glm::vec3& color)
{
    const float half = size * 0.5f;
    std::vector<vertex_pos_color> verts;
    std::vector<uint16_t> idx;
    for (auto& o : k_cube_offsets) verts.push_back({ center + o * half, color });
    for (auto& [i, j] : k_cube_edges) { idx.push_back(static_cast<uint16_t>(i)); idx.push_back(static_cast<uint16_t>(j)); }
    meshes_[next_mesh_handle_] = upload_wireframe_mesh(verts, idx);
    return next_mesh_handle_++;
}

mesh_handle Renderer::create_wireframe_sphere(const glm::vec3& center, float radius, const glm::vec3& color, int seg)
{
    std::vector<vertex_pos_color> verts;
    std::vector<uint16_t> idx;
    auto circle = [&](int a1, int a2) {
        auto base = static_cast<uint16_t>(verts.size());
        for (int i = 0; i <= seg; ++i) {
            float ang = 6.28318f * static_cast<float>(i) / static_cast<float>(seg);
            glm::vec3 p = center; p[a1] += radius * std::cos(ang); p[a2] += radius * std::sin(ang);
            verts.push_back({ p, color });
            if (i > 0) { idx.push_back(static_cast<uint16_t>(base + i - 1)); idx.push_back(static_cast<uint16_t>(base + i)); }
        }
    };
    circle(0, 1); circle(0, 2); circle(1, 2);
    meshes_[next_mesh_handle_] = upload_wireframe_mesh(verts, idx);
    return next_mesh_handle_++;
}

mesh_handle Renderer::create_wireframe_grid(float size, int div, const glm::vec3& color)
{
    std::vector<vertex_pos_color> verts;
    std::vector<uint16_t> idx;
    const float half = size * 0.5f, step = size / static_cast<float>(div);
    for (int i = 0; i <= div; ++i) {
        float t = -half + step * static_cast<float>(i);
        auto b = static_cast<uint16_t>(verts.size());
        verts.push_back({ {t, 0, -half}, color }); verts.push_back({ {t, 0, half}, color });
        verts.push_back({ {-half, 0, t}, color }); verts.push_back({ {half, 0, t}, color });
        idx.push_back(b); idx.push_back(static_cast<uint16_t>(b+1)); idx.push_back(static_cast<uint16_t>(b+2)); idx.push_back(static_cast<uint16_t>(b+3));
    }
    meshes_[next_mesh_handle_] = upload_wireframe_mesh(verts, idx);
    return next_mesh_handle_++;
}

mesh_handle Renderer::create_mesh(std::span<const vertex> verts, std::span<const uint16_t> idx, [[maybe_unused]] primitive_type)
{
    if (verts.empty() || idx.empty()) return invalid_mesh;
    std::vector<vertex_pos_color> v(verts.size());
    for (size_t i = 0; i < verts.size(); ++i) v[i] = { verts[i].position, verts[i].color };
    meshes_[next_mesh_handle_] = upload_wireframe_mesh(v, idx);
    return next_mesh_handle_++;
}

void Renderer::destroy_mesh(mesh_handle h)
{
    if (auto it = meshes_.find(h); it != meshes_.end()) {
        if (it->second.vertex_buffer) SDL_ReleaseGPUBuffer(device_, it->second.vertex_buffer);
        if (it->second.index_buffer)  SDL_ReleaseGPUBuffer(device_, it->second.index_buffer);
        meshes_.erase(it);
    }
}

void Renderer::draw_mesh_internal(const gpu_mesh& m)
{
    if (!current_pass_ || !current_cmd_) return;
    
    uniform_mvp uniforms{ view_proj_ };
    SDL_PushGPUVertexUniformData(current_cmd_, 0, &uniforms, sizeof(uniforms));

    SDL_GPUBufferBinding vb{};
    vb.buffer = m.vertex_buffer;
    vb.offset = 0;
    SDL_BindGPUVertexBuffers(current_pass_, 0, &vb, 1);
    
    SDL_GPUBufferBinding ib{};
    ib.buffer = m.index_buffer;
    ib.offset = 0;
    SDL_BindGPUIndexBuffer(current_pass_, &ib, SDL_GPU_INDEXELEMENTSIZE_16BIT);
    SDL_DrawGPUIndexedPrimitives(current_pass_, m.index_count, 1, 0, 0, 0);
}

void Renderer::draw(mesh_handle h)
{
    if (auto it = meshes_.find(h); it != meshes_.end()) {
        if (wireframe_pipeline_ && current_pass_)
            SDL_BindGPUGraphicsPipeline(current_pass_, wireframe_pipeline_);
        draw_mesh_internal(it->second);
    }
}

std::filesystem::path Renderer::find_texture_for_model(const std::filesystem::path& model_path)
{
    auto dir = model_path.parent_path();
    auto stem = model_path.stem();
    
    for (const auto& ext : { ".tga", ".TGA", ".png", ".PNG", ".jpg", ".JPG", ".jpeg" })
    {
        auto tex_path = dir / (stem.string() + ext);
        if (std::filesystem::exists(tex_path)) return tex_path;
    }
    
    for (const auto& entry : std::filesystem::directory_iterator(dir))
    {
        if (!entry.is_regular_file()) continue;
        auto ext = entry.path().extension().string();
        if (ext == ".tga" || ext == ".TGA" || ext == ".png" || ext == ".PNG" || ext == ".jpg" || ext == ".JPG")
            return entry.path();
    }
    
    return {};
}

texture_handle Renderer::load_texture(const std::filesystem::path& path)
{
    auto result = as3::load_texture(device_, path, true);
    if (!result)
    {
        spdlog::error("== texture {}: {}", path.string(), result.error());
        return invalid_texture;
    }
    
    textures_[next_texture_handle_] = { result->texture, result->sampler, result->width, result->height };
    spdlog::info("=> texture: {} ({}x{})", path.filename().string(), result->width, result->height);
    return next_texture_handle_++;
}

void Renderer::unload_texture(texture_handle h)
{
    if (h == default_texture_) return;
    if (auto it = textures_.find(h); it != textures_.end())
    {
        if (it->second.texture) SDL_ReleaseGPUTexture(device_, it->second.texture);
        if (it->second.sampler) SDL_ReleaseGPUSampler(device_, it->second.sampler);
        textures_.erase(it);
    }
}

model_handle Renderer::load_model(const std::filesystem::path& path, const glm::vec3& color)
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.c_str())) {
        spdlog::error("== obj {}: {}", path.string(), err);
        return invalid_model;
    }
    

    gpu_model model;
    model.color = color;
    model.has_uvs = !attrib.texcoords.empty();
    
    model.model_bounds.min = glm::vec3(std::numeric_limits<float>::max());
    model.model_bounds.max = glm::vec3(std::numeric_limits<float>::lowest());
    for (size_t i = 0; i < attrib.vertices.size(); i += 3)
    {
        glm::vec3 v(attrib.vertices[i], attrib.vertices[i+1], attrib.vertices[i+2]);
        model.model_bounds.min = glm::min(model.model_bounds.min, v);
        model.model_bounds.max = glm::max(model.model_bounds.max, v);
    }
    
    if (auto tex_path = find_texture_for_model(path); !tex_path.empty())
        model.texture = load_texture(tex_path);

    for (const auto& shape : shapes)
    {
        std::vector<vertex_textured> verts;
        std::vector<uint16_t> idx;

        size_t index_offset = 0;
        for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); ++f)
        {
            auto fv = shape.mesh.num_face_vertices[f];
            if (fv != 3) { index_offset += static_cast<size_t>(fv); continue; }

            for (int v = 0; v < 3; ++v)
            {
                auto i = shape.mesh.indices[index_offset + static_cast<size_t>(v)];
                
                // Validate indices
                if (i.vertex_index < 0 || static_cast<size_t>(i.vertex_index * 3 + 2) >= attrib.vertices.size())
                    continue;
                
                vertex_textured vert{};
                vert.position = glm::vec3(
                    attrib.vertices[static_cast<size_t>(3 * i.vertex_index + 0)],
                    attrib.vertices[static_cast<size_t>(3 * i.vertex_index + 1)],
                    attrib.vertices[static_cast<size_t>(3 * i.vertex_index + 2)]
                );
                
                if (i.normal_index >= 0 && static_cast<size_t>(i.normal_index * 3 + 2) < attrib.normals.size())
                    vert.normal = glm::vec3(
                        attrib.normals[static_cast<size_t>(3 * i.normal_index + 0)],
                        attrib.normals[static_cast<size_t>(3 * i.normal_index + 1)],
                        attrib.normals[static_cast<size_t>(3 * i.normal_index + 2)]
                    );
                else
                    vert.normal = glm::vec3(0.0f, 1.0f, 0.0f);
                
                if (i.texcoord_index >= 0 && static_cast<size_t>(i.texcoord_index * 2 + 1) < attrib.texcoords.size())
                    vert.texcoord = glm::vec2(
                        attrib.texcoords[static_cast<size_t>(2 * i.texcoord_index + 0)],
                        attrib.texcoords[static_cast<size_t>(2 * i.texcoord_index + 1)]
                    );
                
                idx.push_back(static_cast<uint16_t>(verts.size()));
                verts.push_back(vert);
            }
            index_offset += 3;
        }
        
        if (!verts.empty() && !idx.empty())
            model.meshes.push_back(upload_textured_mesh(verts, idx));
    }

    if (model.meshes.empty()) {
        spdlog::error("== obj {}: no meshes", path.string());
        return invalid_model;
    }

    auto h = next_model_handle_++;
    models_[h] = std::move(model);
    spdlog::info("=> model: {} ({} meshes)", path.filename().string(), models_[h].meshes.size());
    return h;
}

void Renderer::unload_model(model_handle h)
{
    if (auto it = models_.find(h); it != models_.end()) {
        for (auto& m : it->second.meshes) {
            if (m.vertex_buffer) SDL_ReleaseGPUBuffer(device_, m.vertex_buffer);
            if (m.index_buffer)  SDL_ReleaseGPUBuffer(device_, m.index_buffer);
        }
        if (it->second.texture != invalid_texture)
            unload_texture(it->second.texture);
        models_.erase(it);
    }
}

void Renderer::draw_textured_mesh_internal(const gpu_textured_mesh& m, texture_handle tex_handle)
{
    if (!current_pass_ || !current_cmd_) return;
    
    auto tex_it = textures_.find(tex_handle);
    if (tex_it == textures_.end()) tex_it = textures_.find(default_texture_);
    if (tex_it == textures_.end()) return;
    
    SDL_GPUTextureSamplerBinding tsb{};
    tsb.texture = tex_it->second.texture;
    tsb.sampler = tex_it->second.sampler;
    SDL_BindGPUFragmentSamplers(current_pass_, 0, &tsb, 1);
    
    SDL_GPUBufferBinding vb{};
    vb.buffer = m.vertex_buffer;
    vb.offset = 0;
    SDL_BindGPUVertexBuffers(current_pass_, 0, &vb, 1);
    
    SDL_GPUBufferBinding ib{};
    ib.buffer = m.index_buffer;
    ib.offset = 0;
    SDL_BindGPUIndexBuffer(current_pass_, &ib, SDL_GPU_INDEXELEMENTSIZE_16BIT);
    SDL_DrawGPUIndexedPrimitives(current_pass_, m.index_count, 1, 0, 0, 0);
}

void Renderer::draw_model(model_handle h, const transform& xform)
{
    auto it = models_.find(h);
    if (it == models_.end()) return;
    
    const auto& model = it->second;

    glm::mat4 model_mat = glm::mat4(1.0f);
    model_mat = glm::translate(model_mat, xform.position);
    model_mat = glm::rotate(model_mat, glm::radians(xform.rotation.x), glm::vec3(1, 0, 0));
    model_mat = glm::rotate(model_mat, glm::radians(xform.rotation.y), glm::vec3(0, 1, 0));
    model_mat = glm::rotate(model_mat, glm::radians(xform.rotation.z), glm::vec3(0, 0, 1));
    model_mat = glm::scale(model_mat, xform.scale);

    auto* pipeline = (render_mode_ == render_mode::wireframe) ? textured_wireframe_pipeline_ : textured_pipeline_;
    if (pipeline && current_pass_)
        SDL_BindGPUGraphicsPipeline(current_pass_, pipeline);
    
    uniform_mvp uniforms{ view_proj_ * model_mat };
    SDL_PushGPUVertexUniformData(current_cmd_, 0, &uniforms, sizeof(uniforms));
    
    auto tex = model.texture != invalid_texture ? model.texture : default_texture_;
    for (const auto& m : model.meshes)
        draw_textured_mesh_internal(m, tex);
}

bounds Renderer::get_bounds(model_handle h) const
{
    if (auto it = models_.find(h); it != models_.end())
        return it->second.model_bounds;
    return {};
}

} // namespace as3
