#pragma once

/// @file obj_loader.hpp
/// @brief Wavefront OBJ model loader

#include "model_loader.hpp"

namespace euengine
{

/// OBJ file format loader
class ObjLoader final : public IModelLoader
{
public:
    ObjLoader()           = default;
    ~ObjLoader() override = default;

    // Non-copyable, non-movable
    ObjLoader(const ObjLoader&)            = delete;
    ObjLoader& operator=(const ObjLoader&) = delete;
    ObjLoader(ObjLoader&&)                 = delete;
    ObjLoader& operator=(ObjLoader&&)      = delete;

    [[nodiscard]] load_result load(
        const std::filesystem::path& path) const override;

    [[nodiscard]] bool supports(std::string_view extension) const override;

    [[nodiscard]] std::span<const std::string_view> extensions() const override;

private:
    static constexpr std::array<std::string_view, 1> k_extensions { ".obj" };
};

} // namespace euengine
