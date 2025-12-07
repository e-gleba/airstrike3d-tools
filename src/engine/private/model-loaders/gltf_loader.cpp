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

// Coordinate system conversion matrix
// glTF/Godot: Y-up, -Z forward (right-handed)
// Engine: Y-up, +Z forward
// We need to flip X and Z to convert between coordinate systems
constexpr glm::mat4 k_coord_convert {
    -1.0f, 0.0f, 0.0f,  0.0f,  // flip X
    0.0f,  1.0f, 0.0f,  0.0f,  // Y unchanged
    0.0f,  0.0f, -1.0f, 0.0f,  // flip Z
    0.0f,  0.0f, 0.0f,  1.0f
};

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

    // Find root nodes (nodes not referenced as children)
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

    // Also check default scene for root nodes
    std::vector<int> roots;
    if (model.defaultScene >= 0 &&
        static_cast<size_t>(model.defaultScene) < model.scenes.size())
    {
        const auto& scene = model.scenes[static_cast<size_t>(model.defaultScene)];
        roots = scene.nodes;
    }
    else
    {
        // Use nodes that aren't children
        for (size_t i = 0; i < model.nodes.size(); ++i)
        {
            if (!is_child[i])
            {
                roots.push_back(static_cast<int>(i));
            }
        }
    }

    // BFS to compute world transforms
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

        const auto& node        = model.nodes[static_cast<size_t>(node_idx)];
        glm::mat4   local       = get_node_transform(node);
        glm::mat4   world       = parent_transform * local;
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

    // Combined transform: coordinate conversion + node world transform
    glm::mat4 final_transform = k_coord_convert * transform;
    
    // Normal matrix for transforming normals (inverse transpose of 3x3)
    glm::mat3 normal_matrix = glm::transpose(glm::inverse(glm::mat3(final_transform)));

    // Build vertices with transform applied
    mesh.vertices.reserve(mesh.vertices.size() + vertex_count);
    for (std::size_t i = 0; i < vertex_count; ++i)
    {
        model_vertex vert {};

        // Transform position
        glm::vec4 pos(positions[i * 3],
                      positions[(i * 3) + 1],
                      positions[(i * 3) + 2],
                      1.0f);
        glm::vec4 transformed_pos = final_transform * pos;
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

    // Build indices - reverse winding order due to coordinate flip
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

        // Read indices into temp buffer
        std::vector<uint16_t> temp_indices;
        temp_indices.reserve(accessor.count);
        
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
            temp_indices.push_back(static_cast<uint16_t>(base_index + idx));
        }
        
        // Reverse winding order (swap every 2nd and 3rd index in each triangle)
        mesh.indices.reserve(mesh.indices.size() + temp_indices.size());
        for (size_t i = 0; i + 2 < temp_indices.size(); i += 3)
        {
            mesh.indices.push_back(temp_indices[i]);
            mesh.indices.push_back(temp_indices[i + 2]); // swapped
            mesh.indices.push_back(temp_indices[i + 1]); // swapped
        }
    }
    else
    {
        // No indices - generate sequential with reversed winding
        mesh.indices.reserve(mesh.indices.size() + vertex_count);
        for (std::size_t i = 0; i + 2 < vertex_count; i += 3)
        {
            mesh.indices.push_back(static_cast<uint16_t>(base_index + i));
            mesh.indices.push_back(static_cast<uint16_t>(base_index + i + 2));
            mesh.indices.push_back(static_cast<uint16_t>(base_index + i + 1));
        }
    }
}

/// Extract embedded texture from GLB
[[nodiscard]] std::filesystem::path extract_embedded_texture(
    [[maybe_unused]] const tinygltf::Model& model,
    const tinygltf::Image& img,
    const std::filesystem::path& base_path)
{
    // If image has embedded data, save to temp file
    if (!img.image.empty())
    {
        // Determine extension
        std::string ext = ".png";
        if (img.mimeType == "image/jpeg" || img.mimeType == "image/jpg")
            ext = ".jpg";
        else if (img.mimeType == "image/png")
            ext = ".png";
        
        // Create temp path
        auto temp_path = base_path.parent_path() / 
            (base_path.stem().string() + "_" + img.name + ext);
        
        // Check if already extracted
        if (std::filesystem::exists(temp_path))
            return temp_path;
        
        // Write image data (already decoded by tinygltf)
        // For now, skip embedded textures - they need encoding
        // Just log that we found one
        spdlog::debug("Found embedded texture: {} ({}x{}, {} components)",
                      img.name, img.width, img.height, img.component);
    }
    
    return {};
}

/// Find best texture from materials
[[nodiscard]] std::filesystem::path find_texture(
    const tinygltf::Model& model,
    const std::filesystem::path& base_path)
{
    // Try materials first
    for (const auto& mat : model.materials)
    {
        int tex_idx = mat.pbrMetallicRoughness.baseColorTexture.index;
        if (tex_idx >= 0 &&
            static_cast<size_t>(tex_idx) < model.textures.size())
        {
            const auto& tex = model.textures[static_cast<size_t>(tex_idx)];
            if (tex.source >= 0 &&
                static_cast<size_t>(tex.source) < model.images.size())
            {
                const auto& img =
                    model.images[static_cast<size_t>(tex.source)];
                
                // External URI
                if (!img.uri.empty())
                {
                    auto tex_path = base_path.parent_path() / img.uri;
                    if (std::filesystem::exists(tex_path))
                        return tex_path;
                }
                
                // Embedded texture
                auto embedded = extract_embedded_texture(model, img, base_path);
                if (!embedded.empty())
                    return embedded;
            }
        }
    }

    // Fallback: first image with URI
    for (const auto& img : model.images)
    {
        if (!img.uri.empty())
        {
            auto tex_path = base_path.parent_path() / img.uri;
            if (std::filesystem::exists(tex_path))
                return tex_path;
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
        ret =
            loader.LoadBinaryFromFile(&gltf_model, &err, &warn, path.string());
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

    // Find texture
    model.texture_path = find_texture(gltf_model, path);

    // Compute world transforms for all nodes
    auto world_transforms = compute_world_transforms(gltf_model);

    // Single mesh to collect all geometry
    loaded_mesh combined {};
    combined.material_name = path.stem().string();

    // Count nodes with meshes for logging
    int mesh_node_count = 0;
    int total_primitives = 0;

    // Process nodes that have meshes, applying their world transforms
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
            total_primitives++;
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

    spdlog::info("=> model (gltf): {} ({} nodes, {} primitives, {} verts)",
                 path.filename().string(),
                 mesh_node_count,
                 total_primitives,
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
