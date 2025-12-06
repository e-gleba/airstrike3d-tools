#pragma once

/// @file texture.hpp
/// @brief GPU texture loading and management utilities

#include <SDL3/SDL_gpu.h>

#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>

namespace euengine
{

/// GPU texture data with sampler
struct texture_data final
{
    SDL_GPUTexture* texture = nullptr;
    SDL_GPUSampler* sampler = nullptr;
    std::int32_t    width   = 0;
    std::int32_t    height  = 0;
};

/// Load texture from file and create GPU resources
/// @param device GPU device to create texture on
/// @param path Path to image file (supports TGA, PNG, JPG, etc.)
/// @param flip_vertical Whether to flip the image vertically (default: true for
/// OpenGL coord system)
/// @return Texture data on success, error message on failure
[[nodiscard]] std::expected<texture_data, std::string> load_texture(
    SDL_GPUDevice*               device,
    const std::filesystem::path& path,
    bool                         flip_vertical = true);

/// Create a 1x1 white texture for fallback/placeholder use
/// @param device GPU device to create texture on
/// @return Texture data on success, error message on failure
[[nodiscard]] std::expected<texture_data, std::string> create_default_texture(
    SDL_GPUDevice* device);

/// Release texture GPU resources
/// @param device GPU device that owns the texture
/// @param tex Texture data to release (will be zeroed after release)
void release_texture(SDL_GPUDevice* device, texture_data& tex);

} // namespace euengine
