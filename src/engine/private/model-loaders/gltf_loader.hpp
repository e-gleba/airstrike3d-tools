#pragma once

/// @file gltf_loader.hpp
/// @brief glTF 2.0 model loader (supports .gltf and .glb)

#include "model_loader.hpp"

namespace euengine
{

/// glTF 2.0 file format loader
class GltfLoader final : public IModelLoader
{
public:
    GltfLoader()           = default;
    ~GltfLoader() override = default;

    // Non-copyable, non-movable
    GltfLoader(const GltfLoader&)            = delete;
    GltfLoader& operator=(const GltfLoader&) = delete;
    GltfLoader(GltfLoader&&)                 = delete;
    GltfLoader& operator=(GltfLoader&&)      = delete;

    [[nodiscard]] load_result load(
        const std::filesystem::path& path) const override;

    [[nodiscard]] bool supports(std::string_view extension) const override;

    [[nodiscard]] std::span<const std::string_view> extensions() const override;

private:
    static constexpr std::array<std::string_view, 2> k_extensions { ".gltf",
                                                                    ".glb" };
};

} // namespace euengine
