#include "gltf_loader.hpp"

// Don't define TINYGLTF_NO_STB_IMAGE - we need tinygltf's image loading
// STB_IMAGE_IMPLEMENTATION is defined in tinygltf_impl.cpp
#include <tiny_gltf.h>

#include <algorithm>
#include <ranges>

namespace as3
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

/// Process a single primitive into vertices and indices
void process_primitive(const tinygltf::Model&     model,
                       const tinygltf::Primitive& primitive,
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

    if (!positions || vertex_count == 0)
        return;

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

    // Build vertices
    mesh.vertices.reserve(mesh.vertices.size() + vertex_count);
    for (std::size_t i = 0; i < vertex_count; ++i)
    {
        model_vertex vert{};
        vert.position = glm::vec3(
            positions[i * 3], positions[i * 3 + 1], positions[i * 3 + 2]);
        bounds.expand(vert.position);

        if (normals)
            vert.normal = glm::vec3(
                normals[i * 3], normals[i * 3 + 1], normals[i * 3 + 2]);

        if (texcoords)
            vert.texcoord = glm::vec2(texcoords[i * 2], texcoords[i * 2 + 1]);

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
            mesh.indices.push_back(static_cast<uint16_t>(base_index + i));
    }
}

} // namespace

load_result GltfLoader::load(const std::filesystem::path& path) const
{
    if (!std::filesystem::exists(path))
        return std::unexpected("file not found: " + path.string());

    tinygltf::Model    gltf_model;
    tinygltf::TinyGLTF loader;
    std::string        err, warn;

    const auto ext = path.extension().string();
    bool       ret = false;

    if (ext == ".glb" || ext == ".GLB")
        ret =
            loader.LoadBinaryFromFile(&gltf_model, &err, &warn, path.string());
    else
        ret = loader.LoadASCIIFromFile(&gltf_model, &err, &warn, path.string());

    if (!ret)
        return std::unexpected("glTF parse error: " + err);

    loaded_model model{};
    model.has_uvs = true;

    // Extract texture path from first image if available
    if (!gltf_model.images.empty())
    {
        const auto& img = gltf_model.images[0];
        if (!img.uri.empty())
        {
            auto tex_path = path.parent_path() / img.uri;
            if (std::filesystem::exists(tex_path))
                model.texture_path = tex_path;
        }
    }

    // Process all meshes
    for (const auto& mesh : gltf_model.meshes)
    {
        loaded_mesh loaded{};
        loaded.material_name = mesh.name;

        for (const auto& primitive : mesh.primitives)
        {
            if (primitive.mode != TINYGLTF_MODE_TRIANGLES)
                continue;
            process_primitive(gltf_model, primitive, loaded, model.bounds);
        }

        if (!loaded.vertices.empty() && !loaded.indices.empty())
            model.meshes.push_back(std::move(loaded));
    }

    if (model.meshes.empty())
        return std::unexpected("glTF file contains no valid meshes");

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

} // namespace as3