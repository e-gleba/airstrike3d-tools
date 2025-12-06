#pragma once

/// @file model_loader.hpp
/// @brief Abstract interface for model loaders supporting multiple formats

#include <glm/glm.hpp>

#include <expected>
#include <filesystem>
#include <limits>
#include <span>
#include <string>
#include <vector>

namespace euengine
{

/// Vertex with position, normal, and texture coordinates
struct model_vertex final
{
    glm::vec3 position {};
    glm::vec3 normal { 0.0f, 1.0f, 0.0f };
    glm::vec2 texcoord {};
};

/// Axis-aligned bounding box
struct aabb final
{
    glm::vec3 min { std::numeric_limits<float>::max() };
    glm::vec3 max { std::numeric_limits<float>::lowest() };

    [[nodiscard]] constexpr glm::vec3 center() const noexcept
    {
        return (min + max) * 0.5f;
    }

    [[nodiscard]] constexpr glm::vec3 extents() const noexcept
    {
        return (max - min) * 0.5f;
    }

    [[nodiscard]] constexpr glm::vec3 size() const noexcept
    {
        return max - min;
    }

    constexpr void expand(const glm::vec3& point) noexcept
    {
        min = glm::min(min, point);
        max = glm::max(max, point);
    }

    [[nodiscard]] constexpr bool valid() const noexcept
    {
        return min.x <= max.x && min.y <= max.y && min.z <= max.z;
    }
};

/// Single mesh within a model
struct loaded_mesh final
{
    std::vector<model_vertex> vertices;
    std::vector<uint16_t>     indices;
    std::string               material_name;
};

/// Complete loaded model data
struct loaded_model final
{
    std::vector<loaded_mesh> meshes;
    std::filesystem::path    texture_path;
    aabb                     bounds;
    bool                     has_uvs = false;

    [[nodiscard]] bool empty() const noexcept { return meshes.empty(); }

    [[nodiscard]] std::size_t total_vertices() const noexcept
    {
        std::size_t total = 0;
        for (const auto& mesh : meshes)
            total += mesh.vertices.size();
        return total;
    }

    [[nodiscard]] std::size_t total_indices() const noexcept
    {
        std::size_t total = 0;
        for (const auto& mesh : meshes)
            total += mesh.indices.size();
        return total;
    }
};

/// Model loader error types
enum class loader_error
{
    file_not_found,
    parse_error,
    invalid_format,
    no_meshes,
    unsupported_feature,
};

/// Convert loader error to string
[[nodiscard]] constexpr std::string_view to_string(loader_error err) noexcept
{
    switch (err)
    {
        case loader_error::file_not_found:
            return "file not found";
        case loader_error::parse_error:
            return "parse error";
        case loader_error::invalid_format:
            return "invalid format";
        case loader_error::no_meshes:
            return "no meshes in file";
        case loader_error::unsupported_feature:
            return "unsupported feature";
    }
    return "unknown error";
}

/// Result type for model loading operations
using load_result = std::expected<loaded_model, std::string>;

/// Abstract base class for model loaders
class IModelLoader
{
public:
    virtual ~IModelLoader() = default;

    /// Load a model from file
    [[nodiscard]] virtual load_result load(
        const std::filesystem::path& path) const = 0;

    /// Check if this loader supports the given file extension
    [[nodiscard]] virtual bool supports(std::string_view extension) const = 0;

    /// Get supported extensions for this loader
    [[nodiscard]] virtual std::span<const std::string_view> extensions()
        const = 0;
};

} // namespace euengine
