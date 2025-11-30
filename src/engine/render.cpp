#include "render.hpp"
#include "shader.hpp"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <glm/gtc/matrix_transform.hpp>
#include <spdlog/spdlog.h>

#include <array>
#include <cstring>

namespace as3
{

namespace
{
constexpr std::array<glm::vec3, 8> k_cube_offsets = { {
    glm::vec3(-1, -1, -1), glm::vec3(1, -1, -1), glm::vec3(1, 1, -1), glm::vec3(-1, 1, -1),
    glm::vec3(-1, -1,  1), glm::vec3(1, -1,  1), glm::vec3(1, 1,  1), glm::vec3(-1, 1,  1),
} };

constexpr std::array<std::pair<size_t, size_t>, 12> k_cube_edges = { {
    {0,1}, {1,2}, {2,3}, {3,0}, {4,5}, {5,6}, {6,7}, {7,4}, {0,4}, {1,5}, {2,6}, {3,7},
} };
} // namespace

Renderer::~Renderer() { shutdown(); }

bool Renderer::init(SDL_GPUDevice* device, ShaderManager* shaders)
{
    device_  = device;
    shaders_ = shaders;

    const ShaderProgramDesc desc{
        .name     = "wireframe",
        .vertex   = { .path = "wireframe.vert.hlsl", .stage = ShaderStage::Vertex },
        .fragment = { .path = "wireframe.frag.hlsl", .stage = ShaderStage::Fragment },
    };

    if (auto r = shaders_->load_program(desc); !r)
    {
        spdlog::error("Failed to load shader: {}", r.error());
        return false;
    }

    shaders_->set_reload_callback([this](const std::string& n) {
        if (n == "wireframe") pipeline_dirty_ = true;
    });

    return create_pipeline();
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

    if (pipeline_) { SDL_ReleaseGPUGraphicsPipeline(device_, pipeline_); pipeline_ = nullptr; }
}

bool Renderer::create_pipeline()
{
    auto* prog = shaders_->get_program("wireframe");
    if (!prog || !prog->valid()) return false;

    SDL_GPUVertexAttribute attrs[2] = {};
    attrs[0].location    = 0;
    attrs[0].buffer_slot = 0;
    attrs[0].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    attrs[0].offset      = 0;
    attrs[1].location    = 1;
    attrs[1].buffer_slot = 0;
    attrs[1].format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    attrs[1].offset      = sizeof(glm::vec3);

    SDL_GPUVertexBufferDescription vb{};
    vb.slot       = 0;
    vb.pitch      = sizeof(vertex_pos_color);
    vb.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

    SDL_GPUVertexInputState vi{};
    vi.vertex_buffer_descriptions = &vb;
    vi.num_vertex_buffers         = 1;
    vi.vertex_attributes          = attrs;
    vi.num_vertex_attributes      = 2;

    SDL_GPUColorTargetDescription ct{};
    ct.format = SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;

    SDL_GPUGraphicsPipelineTargetInfo ti{};
    ti.color_target_descriptions = &ct;
    ti.num_color_targets         = 1;
    ti.depth_stencil_format      = SDL_GPU_TEXTUREFORMAT_INVALID;

    SDL_GPURasterizerState rs{};
    rs.fill_mode  = SDL_GPU_FILLMODE_FILL;
    rs.cull_mode  = SDL_GPU_CULLMODE_NONE;
    rs.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;

    SDL_GPUMultisampleState ms{};
    ms.sample_count = SDL_GPU_SAMPLECOUNT_1;

    SDL_GPUGraphicsPipelineCreateInfo info{};
    info.vertex_shader      = prog->vertex_shader();
    info.fragment_shader    = prog->fragment_shader();
    info.vertex_input_state = vi;
    info.primitive_type     = SDL_GPU_PRIMITIVETYPE_LINELIST;
    info.rasterizer_state   = rs;
    info.multisample_state  = ms;
    info.target_info        = ti;

    if (pipeline_) SDL_ReleaseGPUGraphicsPipeline(device_, pipeline_);
    pipeline_ = SDL_CreateGPUGraphicsPipeline(device_, &info);
    pipeline_dirty_ = false;
    if (pipeline_) spdlog::info("Pipeline created");
    return pipeline_ != nullptr;
}

void Renderer::reload_pipeline() { if (pipeline_dirty_) create_pipeline(); }

void Renderer::begin_frame(SDL_GPUCommandBuffer* cmd, SDL_GPURenderPass* pass)
{
    current_cmd_ = cmd; current_pass_ = pass;
    reload_pipeline();
}

void Renderer::end_frame() { current_cmd_ = nullptr; current_pass_ = nullptr; }

void Renderer::set_view_projection(const glm::mat4& vp) { view_proj_ = vp; }

void Renderer::bind_pipeline() { if (pipeline_ && current_pass_) SDL_BindGPUGraphicsPipeline(current_pass_, pipeline_); }

gpu_mesh Renderer::upload_mesh(std::span<const vertex_pos_color> verts, std::span<const uint16_t> idx)
{
    gpu_mesh mesh{};
    mesh.vertex_count = static_cast<Uint32>(verts.size());
    mesh.index_count  = static_cast<Uint32>(idx.size());

    auto vb_size = static_cast<Uint32>(verts.size_bytes());
    auto ib_size = static_cast<Uint32>(idx.size_bytes());

    SDL_GPUBufferCreateInfo vb_info{};
    vb_info.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    vb_info.size  = vb_size;
    mesh.vertex_buffer = SDL_CreateGPUBuffer(device_, &vb_info);

    SDL_GPUBufferCreateInfo ib_info{};
    ib_info.usage = SDL_GPU_BUFFERUSAGE_INDEX;
    ib_info.size  = ib_size;
    mesh.index_buffer = SDL_CreateGPUBuffer(device_, &ib_info);

    SDL_GPUTransferBufferCreateInfo tb_info{};
    tb_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tb_info.size  = vb_size + ib_size;
    auto* tb = SDL_CreateGPUTransferBuffer(device_, &tb_info);

    auto* ptr = SDL_MapGPUTransferBuffer(device_, tb, false);
    std::memcpy(ptr, verts.data(), vb_size);
    std::memcpy(static_cast<char*>(ptr) + vb_size, idx.data(), ib_size);
    SDL_UnmapGPUTransferBuffer(device_, tb);

    auto* cmd = SDL_AcquireGPUCommandBuffer(device_);
    auto* cp  = SDL_BeginGPUCopyPass(cmd);

    SDL_GPUTransferBufferLocation src_loc{};
    src_loc.transfer_buffer = tb;
    src_loc.offset = 0;

    SDL_GPUBufferRegion vb_region{};
    vb_region.buffer = mesh.vertex_buffer;
    vb_region.offset = 0;
    vb_region.size   = vb_size;

    SDL_UploadToGPUBuffer(cp, &src_loc, &vb_region, false);

    SDL_GPUTransferBufferLocation src_loc2{};
    src_loc2.transfer_buffer = tb;
    src_loc2.offset = vb_size;

    SDL_GPUBufferRegion ib_region{};
    ib_region.buffer = mesh.index_buffer;
    ib_region.offset = 0;
    ib_region.size   = ib_size;

    SDL_UploadToGPUBuffer(cp, &src_loc2, &ib_region, false);

    SDL_EndGPUCopyPass(cp);
    SDL_SubmitGPUCommandBuffer(cmd);
    SDL_ReleaseGPUTransferBuffer(device_, tb);

    return mesh;
}

mesh_handle Renderer::create_wireframe_cube(const glm::vec3& center, float size, const glm::vec3& color)
{
    float half = size * 0.5f;
    std::vector<vertex_pos_color> verts;
    std::vector<uint16_t> idx;
    for (auto& o : k_cube_offsets) verts.push_back({ center + o * half, color });
    for (auto& [i, j] : k_cube_edges) { idx.push_back(static_cast<uint16_t>(i)); idx.push_back(static_cast<uint16_t>(j)); }
    auto h = next_mesh_handle_++;
    meshes_[h] = upload_mesh(verts, idx);
    return h;
}

mesh_handle Renderer::create_wireframe_sphere(const glm::vec3& center, float radius, const glm::vec3& color, int seg)
{
    std::vector<vertex_pos_color> verts;
    std::vector<uint16_t> idx;
    auto circle = [&](int a1, int a2) {
        uint16_t base = static_cast<uint16_t>(verts.size());
        for (int i = 0; i <= seg; ++i) {
            float ang = 6.28318f * static_cast<float>(i) / static_cast<float>(seg);
            glm::vec3 p = center; p[a1] += radius * std::cos(ang); p[a2] += radius * std::sin(ang);
            verts.push_back({ p, color });
            if (i > 0) { idx.push_back(static_cast<uint16_t>(base + i - 1)); idx.push_back(static_cast<uint16_t>(base + i)); }
        }
    };
    circle(0, 1); circle(0, 2); circle(1, 2);
    auto h = next_mesh_handle_++;
    meshes_[h] = upload_mesh(verts, idx);
    return h;
}

mesh_handle Renderer::create_wireframe_grid(float size, int div, const glm::vec3& color)
{
    std::vector<vertex_pos_color> verts;
    std::vector<uint16_t> idx;
    float half = size * 0.5f, step = size / static_cast<float>(div);
    for (int i = 0; i <= div; ++i) {
        float t = -half + step * static_cast<float>(i);
        uint16_t b = static_cast<uint16_t>(verts.size());
        verts.push_back({ {t, 0, -half}, color }); verts.push_back({ {t, 0, half}, color });
        verts.push_back({ {-half, 0, t}, color }); verts.push_back({ {half, 0, t}, color });
        idx.push_back(b); idx.push_back(static_cast<uint16_t>(b+1)); idx.push_back(static_cast<uint16_t>(b+2)); idx.push_back(static_cast<uint16_t>(b+3));
    }
    auto h = next_mesh_handle_++;
    meshes_[h] = upload_mesh(verts, idx);
    return h;
}

mesh_handle Renderer::create_mesh(std::span<const vertex> verts, std::span<const uint16_t> idx, [[maybe_unused]] primitive_type)
{
    if (verts.empty() || idx.empty()) return invalid_mesh;
    std::vector<vertex_pos_color> v(verts.size());
    for (size_t i = 0; i < verts.size(); ++i) v[i] = { verts[i].position, verts[i].color };
    auto h = next_mesh_handle_++;
    meshes_[h] = upload_mesh(v, idx);
    return h;
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
    if (!pipeline_ || !current_pass_ || !current_cmd_) return;
    SDL_PushGPUVertexUniformData(current_cmd_, 0, &uniforms_, sizeof(uniforms_));

    SDL_GPUBufferBinding vb_binding{};
    vb_binding.buffer = m.vertex_buffer;
    vb_binding.offset = 0;
    SDL_BindGPUVertexBuffers(current_pass_, 0, &vb_binding, 1);

    SDL_GPUBufferBinding ib_binding{};
    ib_binding.buffer = m.index_buffer;
    ib_binding.offset = 0;
    SDL_BindGPUIndexBuffer(current_pass_, &ib_binding, SDL_GPU_INDEXELEMENTSIZE_16BIT);

    SDL_DrawGPUIndexedPrimitives(current_pass_, m.index_count, 1, 0, 0, 0);
}

void Renderer::draw(mesh_handle h)
{
    if (auto it = meshes_.find(h); it != meshes_.end()) {
        uniforms_.mvp = view_proj_;
        draw_mesh_internal(it->second);
    }
}

model_handle Renderer::load_model(const std::filesystem::path& path, const glm::vec3& color)
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.c_str())) {
        spdlog::error("Failed to load OBJ '{}': {}", path.string(), err);
        return invalid_model;
    }
    if (!warn.empty()) spdlog::warn("OBJ '{}': {}", path.string(), warn);

    gpu_model model;
    model.color = color;

    for (const auto& shape : shapes)
    {
        std::vector<vertex_pos_color> verts;
        std::vector<uint16_t> idx;

        size_t index_offset = 0;
        for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); ++f)
        {
            auto fv = shape.mesh.num_face_vertices[f];
            if (fv != 3) { index_offset += static_cast<size_t>(fv); continue; } // skip non-triangles

            std::array<uint16_t, 3> face_idx;
            for (int v = 0; v < 3; ++v)
            {
                auto i = shape.mesh.indices[index_offset + static_cast<size_t>(v)];
                glm::vec3 pos(
                    attrib.vertices[static_cast<size_t>(3 * i.vertex_index + 0)],
                    attrib.vertices[static_cast<size_t>(3 * i.vertex_index + 1)],
                    attrib.vertices[static_cast<size_t>(3 * i.vertex_index + 2)]
                );
                face_idx[static_cast<size_t>(v)] = static_cast<uint16_t>(verts.size());
                verts.push_back({ pos, color });
            }

            // Wireframe: emit edges of triangle
            idx.push_back(face_idx[0]); idx.push_back(face_idx[1]);
            idx.push_back(face_idx[1]); idx.push_back(face_idx[2]);
            idx.push_back(face_idx[2]); idx.push_back(face_idx[0]);

            index_offset += 3;
        }

        if (!verts.empty() && !idx.empty()) {
            model.meshes.push_back(upload_mesh(verts, idx));
        }
    }

    if (model.meshes.empty()) {
        spdlog::error("No valid meshes in '{}'", path.string());
        return invalid_model;
    }

    auto h = next_model_handle_++;
    models_[h] = std::move(model);
    spdlog::info("Loaded model '{}' ({} meshes)", path.string(), models_[h].meshes.size());
    return h;
}

void Renderer::unload_model(model_handle h)
{
    if (auto it = models_.find(h); it != models_.end()) {
        for (auto& m : it->second.meshes) {
            if (m.vertex_buffer) SDL_ReleaseGPUBuffer(device_, m.vertex_buffer);
            if (m.index_buffer)  SDL_ReleaseGPUBuffer(device_, m.index_buffer);
        }
        models_.erase(it);
    }
}

void Renderer::draw_model(model_handle h, const transform& xform)
{
    auto it = models_.find(h);
    if (it == models_.end()) return;

    glm::mat4 model_mat = glm::mat4(1.0f);
    model_mat = glm::translate(model_mat, xform.position);
    model_mat = glm::rotate(model_mat, glm::radians(xform.rotation.x), glm::vec3(1, 0, 0));
    model_mat = glm::rotate(model_mat, glm::radians(xform.rotation.y), glm::vec3(0, 1, 0));
    model_mat = glm::rotate(model_mat, glm::radians(xform.rotation.z), glm::vec3(0, 0, 1));
    model_mat = glm::scale(model_mat, xform.scale);

    uniforms_.mvp = view_proj_ * model_mat;

    for (auto& m : it->second.meshes) {
        draw_mesh_internal(m);
    }
}

} // namespace as3
