/// @file obj_loader.cpp
/// @brief Wavefront OBJ model loader implementation

#include "obj_loader.hpp"

#include <tiny_obj_loader.h>

#include <algorithm>
#include <ranges>

namespace euengine
{

load_result ObjLoader::load(const std::filesystem::path& path) const
{
    if (!std::filesystem::exists(path))
    {
        return std::unexpected("file not found: " + path.string());
    }

    tinyobj::attrib_t                attrib;
    std::vector<tinyobj::shape_t>    shapes;
    std::vector<tinyobj::material_t> materials;
    std::string                      warn;
    std::string                      err;

    if (!tinyobj::LoadObj(
            &attrib, &shapes, &materials, &warn, &err, path.c_str()))
    {
        return std::unexpected("OBJ parse error: " + err);
    }

    loaded_model model {};
    model.has_uvs = !attrib.texcoords.empty();

    // Calculate bounds from all vertices
    for (std::size_t i = 0; i < attrib.vertices.size(); i += 3)
    {
        const glm::vec3 v(
            attrib.vertices[i], attrib.vertices[i + 1], attrib.vertices[i + 2]);
        model.bounds.expand(v);
    }

    // Process each shape into a mesh
    for (const auto& shape : shapes)
    {
        loaded_mesh mesh {};
        mesh.material_name = shape.name;

        std::size_t index_offset = 0;
        for (std::size_t f = 0; f < shape.mesh.num_face_vertices.size(); ++f)
        {
            const auto fv = shape.mesh.num_face_vertices[f];
            if (fv != 3)
            {
                // Skip non-triangular faces
                index_offset += static_cast<std::size_t>(fv);
                continue;
            }

            for (int v = 0; v < 3; ++v)
            {
                const auto& idx =
                    shape.mesh
                        .indices[index_offset + static_cast<std::size_t>(v)];

                // Validate vertex index
                if (idx.vertex_index < 0 ||
                    static_cast<std::size_t>((idx.vertex_index * 3) + 2) >=
                        attrib.vertices.size())
                {
                    continue;
                }

                model_vertex vert {};
                const auto   vi = static_cast<std::size_t>(idx.vertex_index);
                vert.position   = glm::vec3(attrib.vertices[(3 * vi) + 0],
                                          attrib.vertices[(3 * vi) + 1],
                                          attrib.vertices[(3 * vi) + 2]);

                // Normal
                if (idx.normal_index >= 0 &&
                    static_cast<std::size_t>((idx.normal_index * 3) + 2) <
                        attrib.normals.size())
                {
                    const auto ni = static_cast<std::size_t>(idx.normal_index);
                    vert.normal   = glm::vec3(attrib.normals[(3 * ni) + 0],
                                            attrib.normals[(3 * ni) + 1],
                                            attrib.normals[(3 * ni) + 2]);
                }

                // Texture coordinates
                if (idx.texcoord_index >= 0 &&
                    static_cast<std::size_t>((idx.texcoord_index * 2) + 1) <
                        attrib.texcoords.size())
                {
                    const auto ti =
                        static_cast<std::size_t>(idx.texcoord_index);
                    vert.texcoord = glm::vec2(attrib.texcoords[(2 * ti) + 0],
                                              attrib.texcoords[(2 * ti) + 1]);
                }

                mesh.indices.push_back(
                    static_cast<uint16_t>(mesh.vertices.size()));
                mesh.vertices.push_back(vert);
            }
            index_offset += 3;
        }

        if (!mesh.vertices.empty() && !mesh.indices.empty())
        {
            model.meshes.push_back(std::move(mesh));
        }
    }

    if (model.meshes.empty())
    {
        return std::unexpected("OBJ file contains no valid meshes");
    }

    return model;
}

bool ObjLoader::supports(std::string_view extension) const
{
    auto lower = std::string(extension);
    std::ranges::transform(lower, lower.begin(), ::tolower);
    return std::ranges::any_of(k_extensions,
                               [&lower](auto ext) { return ext == lower; });
}

std::span<const std::string_view> ObjLoader::extensions() const
{
    return k_extensions;
}

} // namespace euengine
