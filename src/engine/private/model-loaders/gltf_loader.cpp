#include "gltf_loader.hpp"

// Don't define TINYGLTF_NO_STB_IMAGE - we need tinygltf's image loading
// STB_IMAGE_IMPLEMENTATION is defined in tinygltf_impl.cpp
#include <tiny_gltf.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <ranges>
#include <stack>
#include <unordered_map>

namespace euengine
{

namespace
{

/// Extract buffer data pointer for an accessor
template <typename T>
[[nodiscard]] const T* get_accessor_data(const tinygltf::Model&    model,
                                         const tinygltf::Accessor& accessor)
{
    const auto& view =
        model.bufferViews[static_cast<std::size_t>(accessor.bufferView)];
    const auto& buffer = model.buffers[static_cast<std::size_t>(view.buffer)];
    return reinterpret_cast<const T*>(buffer.data.data() + view.byteOffset +
                                      accessor.byteOffset);
}

/// Get local transform matrix from a glTF node
[[nodiscard]] glm::mat4 get_node_transform(const tinygltf::Node& node)
{
    if (!node.matrix.empty())
    {
        // Matrix is provided directly (column-major)
        return glm::make_mat4(node.matrix.data());
    }

    // Compose from TRS
    glm::mat4 mat { 1.0f };

    if (!node.translation.empty())
    {
        mat = glm::translate(mat,
                             glm::vec3(static_cast<float>(node.translation[0]),
                                       static_cast<float>(node.translation[1]),
                                       static_cast<float>(node.translation[2])));
    }

    if (!node.rotation.empty())
    {
        // glTF quaternion: [x, y, z, w]
        glm::quat q(static_cast<float>(node.rotation[3]),  // w
                    static_cast<float>(node.rotation[0]),  // x
                    static_cast<float>(node.rotation[1]),  // y
                    static_cast<float>(node.rotation[2])); // z
        mat = mat * glm::mat4_cast(q);
    }

    if (!node.scale.empty())
    {
        mat = glm::scale(mat,
                         glm::vec3(static_cast<float>(node.scale[0]),
                                   static_cast<float>(node.scale[1]),
                                   static_cast<float>(node.scale[2])));
    }

    return mat;
}

/// Compute world transforms for all nodes
[[nodiscard]] std::unordered_map<int, glm::mat4> compute_world_transforms(
    const tinygltf::Model& model)
{
    std::unordered_map<int, glm::mat4> world_transforms;

    // Find root nodes
    std::vector<bool> is_child(model.nodes.size(), false);
    for (const auto& node : model.nodes)
    {
        for (int child : node.children)
        {
            if (child >= 0 && static_cast<size_t>(child) < model.nodes.size())
            {
                is_child[static_cast<size_t>(child)] = true;
            }
        }
    }

    // Get root nodes from default scene or find orphans
    std::vector<int> roots;
    if (model.defaultScene >= 0 &&
        static_cast<size_t>(model.defaultScene) < model.scenes.size())
    {
        const auto& scene = model.scenes[static_cast<size_t>(model.defaultScene)];
        roots = scene.nodes;
    }
    else
    {
        for (size_t i = 0; i < model.nodes.size(); ++i)
        {
            if (!is_child[i])
            {
                roots.push_back(static_cast<int>(i));
            }
        }
    }

    // DFS to compute world transforms
    std::stack<std::pair<int, glm::mat4>> stack;
    for (int root : roots)
    {
        stack.push({ root, glm::mat4 { 1.0f } });
    }

    while (!stack.empty())
    {
        auto [node_idx, parent_transform] = stack.top();
        stack.pop();

        if (node_idx < 0 ||
            static_cast<size_t>(node_idx) >= model.nodes.size())
        {
            continue;
        }

        const auto& node  = model.nodes[static_cast<size_t>(node_idx)];
        glm::mat4   local = get_node_transform(node);
        glm::mat4   world = parent_transform * local;
        world_transforms[node_idx] = world;

        for (int child : node.children)
        {
            stack.push({ child, world });
        }
    }

    return world_transforms;
}

/// Process a single primitive with transform applied
void process_primitive(const tinygltf::Model&     model,
                       const tinygltf::Primitive& primitive,
                       const glm::mat4&           transform,
                       loaded_mesh&               mesh,
                       aabb&                      bounds)
{
    const float* positions    = nullptr;
    const float* normals      = nullptr;
    const float* texcoords    = nullptr;
    std::size_t  vertex_count = 0;

    // Position accessor (required)
    if (auto it = primitive.attributes.find("POSITION");
        it != primitive.attributes.end())
    {
        const auto& accessor =
            model.accessors[static_cast<std::size_t>(it->second)];
        positions    = get_accessor_data<float>(model, accessor);
        vertex_count = accessor.count;
    }

    if ((positions == nullptr) || vertex_count == 0)
    {
        return;
    }

    // Normal accessor
    if (auto it = primitive.attributes.find("NORMAL");
        it != primitive.attributes.end())
    {
        const auto& accessor =
            model.accessors[static_cast<std::size_t>(it->second)];
        normals = get_accessor_data<float>(model, accessor);
    }

    // Texcoord accessor
    if (auto it = primitive.attributes.find("TEXCOORD_0");
        it != primitive.attributes.end())
    {
        const auto& accessor =
            model.accessors[static_cast<std::size_t>(it->second)];
        texcoords = get_accessor_data<float>(model, accessor);
    }

    // Normal matrix for transforming normals
    glm::mat3 normal_matrix = glm::transpose(glm::inverse(glm::mat3(transform)));

    // Build vertices with transform applied
    mesh.vertices.reserve(mesh.vertices.size() + vertex_count);
    for (std::size_t i = 0; i < vertex_count; ++i)
    {
        model_vertex vert {};

        // Transform position - use glTF coordinates as-is
        glm::vec4 pos(positions[i * 3],
                      positions[(i * 3) + 1],
                      positions[(i * 3) + 2],
                      1.0f);
        glm::vec4 transformed_pos = transform * pos;
        vert.position = glm::vec3(transformed_pos);
        bounds.expand(vert.position);

        // Transform normal
        if (normals != nullptr)
        {
            glm::vec3 n(normals[i * 3],
                        normals[(i * 3) + 1],
                        normals[(i * 3) + 2]);
            vert.normal = glm::normalize(normal_matrix * n);
        }

        if (texcoords != nullptr)
        {
            vert.texcoord = glm::vec2(texcoords[i * 2], texcoords[(i * 2) + 1]);
        }

        mesh.vertices.push_back(vert);
    }

    // Build indices
    const auto base_index =
        static_cast<uint16_t>(mesh.vertices.size() - vertex_count);

    if (primitive.indices >= 0)
    {
        const auto& accessor =
            model.accessors[static_cast<std::size_t>(primitive.indices)];
        const auto& view =
            model.bufferViews[static_cast<std::size_t>(accessor.bufferView)];
        const auto& buffer =
            model.buffers[static_cast<std::size_t>(view.buffer)];
        const auto* data =
            buffer.data.data() + view.byteOffset + accessor.byteOffset;

        mesh.indices.reserve(mesh.indices.size() + accessor.count);
        for (std::size_t i = 0; i < accessor.count; ++i)
        {
            uint32_t idx = 0;
            switch (accessor.componentType)
            {
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                    idx = static_cast<const uint8_t*>(
                        static_cast<const void*>(data))[i];
                    break;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                    idx = static_cast<const uint16_t*>(
                        static_cast<const void*>(data))[i];
                    break;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                    idx = static_cast<const uint32_t*>(
                        static_cast<const void*>(data))[i];
                    break;
                default:
                    break;
            }
            mesh.indices.push_back(static_cast<uint16_t>(base_index + idx));
        }
    }
    else
    {
        // No indices - generate sequential
        mesh.indices.reserve(mesh.indices.size() + vertex_count);
        for (std::size_t i = 0; i < vertex_count; ++i)
        {
            mesh.indices.push_back(static_cast<uint16_t>(base_index + i));
        }
    }
}

/// Find texture path - handles both external files and embedded textures
[[nodiscard]] std::filesystem::path find_texture(
    const tinygltf::Model&       model,
    const std::filesystem::path& base_path)
{
    // First try to find texture from materials
    for (const auto& mat : model.materials)
    {
        int tex_idx = mat.pbrMetallicRoughness.baseColorTexture.index;
        if (tex_idx < 0 ||
            static_cast<size_t>(tex_idx) >= model.textures.size())
        {
            continue;
        }

        const auto& tex = model.textures[static_cast<size_t>(tex_idx)];
        if (tex.source < 0 ||
            static_cast<size_t>(tex.source) >= model.images.size())
        {
            continue;
        }

        const auto& img = model.images[static_cast<size_t>(tex.source)];

        // External texture file
        if (!img.uri.empty() && img.uri.find("data:") != 0)
        {
            auto tex_path = base_path.parent_path() / img.uri;
            if (std::filesystem::exists(tex_path))
            {
                spdlog::info("  texture: {}", img.uri);
                return tex_path;
            }
            
            // Try without path - just filename
            auto filename_only = std::filesystem::path(img.uri).filename();
            tex_path = base_path.parent_path() / filename_only;
            if (std::filesystem::exists(tex_path))
            {
                spdlog::info("  texture: {}", filename_only.string());
                return tex_path;
            }
        }

        // Embedded texture - tinygltf already decoded it
        if (!img.image.empty())
        {
            spdlog::info("  embedded texture: {}x{} ({} components)",
                         img.width, img.height, img.component);
            // Return empty - embedded textures need special handling
            // For now, we'll handle this in renderer
        }
    }

    // Fallback: look for any image file next to the model
    std::vector<std::string> extensions = { ".png", ".jpg", ".jpeg", ".tga", ".PNG", ".JPG", ".TGA" };
    auto stem = base_path.stem().string();
    
    for (const auto& ext : extensions)
    {
        auto tex_path = base_path.parent_path() / (stem + ext);
        if (std::filesystem::exists(tex_path))
        {
            spdlog::info("  texture (fallback): {}", tex_path.filename().string());
            return tex_path;
        }
    }

    // Check for any texture in same directory
    if (std::filesystem::exists(base_path.parent_path()))
    {
        for (const auto& entry : std::filesystem::directory_iterator(base_path.parent_path()))
        {
            if (!entry.is_regular_file()) continue;
            auto ext = entry.path().extension().string();
            std::ranges::transform(ext, ext.begin(), ::tolower);
            if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga")
            {
                spdlog::info("  texture (dir scan): {}", entry.path().filename().string());
                return entry.path();
            }
        }
    }

    return {};
}

} // namespace

load_result GltfLoader::load(const std::filesystem::path& path) const
{
    if (!std::filesystem::exists(path))
    {
        return std::unexpected("file not found: " + path.string());
    }

    tinygltf::Model    gltf_model;
    tinygltf::TinyGLTF loader;
    std::string        err;
    std::string        warn;

    const auto ext = path.extension().string();
    bool       ret = false;

    if (ext == ".glb" || ext == ".GLB")
    {
        ret = loader.LoadBinaryFromFile(&gltf_model, &err, &warn, path.string());
    }
    else
    {
        ret = loader.LoadASCIIFromFile(&gltf_model, &err, &warn, path.string());
    }

    if (!warn.empty())
    {
        spdlog::warn("glTF warning: {}", warn);
    }

    if (!ret)
    {
        return std::unexpected("glTF parse error: " + err);
    }

    loaded_model model {};
    model.has_uvs = true;

    // Compute world transforms for all nodes
    auto world_transforms = compute_world_transforms(gltf_model);

    // Single mesh to collect all geometry
    loaded_mesh combined {};
    combined.material_name = path.stem().string();

    int mesh_node_count = 0;

    // Process nodes that have meshes
    for (size_t node_idx = 0; node_idx < gltf_model.nodes.size(); ++node_idx)
    {
        const auto& node = gltf_model.nodes[node_idx];
        if (node.mesh < 0)
        {
            continue;
        }

        mesh_node_count++;

        // Get world transform for this node
        glm::mat4 world_transform { 1.0f };
        if (auto it = world_transforms.find(static_cast<int>(node_idx));
            it != world_transforms.end())
        {
            world_transform = it->second;
        }

        const auto& mesh = gltf_model.meshes[static_cast<size_t>(node.mesh)];
        for (const auto& primitive : mesh.primitives)
        {
            if (primitive.mode != TINYGLTF_MODE_TRIANGLES)
            {
                continue;
            }
            process_primitive(gltf_model,
                              primitive,
                              world_transform,
                              combined,
                              model.bounds);
        }
    }

    if (!combined.vertices.empty() && !combined.indices.empty())
    {
        model.meshes.push_back(std::move(combined));
    }

    if (model.meshes.empty())
    {
        return std::unexpected("glTF file contains no valid meshes");
    }

    // Find texture
    model.texture_path = find_texture(gltf_model, path);

    spdlog::info("=> model (gltf): {} ({} meshes, {} verts)",
                 path.filename().string(),
                 mesh_node_count,
                 model.meshes[0].vertices.size());

    return model;
}

bool GltfLoader::supports(std::string_view extension) const
{
    auto lower = std::string(extension);
    std::ranges::transform(lower, lower.begin(), ::tolower);
    return std::ranges::any_of(k_extensions,
                               [&lower](auto ext) { return ext == lower; });
}

std::span<const std::string_view> GltfLoader::extensions() const
{
    return k_extensions;
}

} // namespace euengine
