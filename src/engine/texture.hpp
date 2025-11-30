#pragma once

#include <SDL3/SDL_gpu.h>

#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>

namespace as3
{

struct texture_data final
{
    SDL_GPUTexture* texture = nullptr;
    SDL_GPUSampler* sampler = nullptr;
    int32_t         width   = 0;
    int32_t         height  = 0;
};

/// Load texture from file, create GPU texture and sampler
[[nodiscard]] std::expected<texture_data, std::string> load_texture(
    SDL_GPUDevice* device,
    const std::filesystem::path& path,
    bool flip_vertical = true
);

/// Create a 1x1 white texture for fallback
[[nodiscard]] std::expected<texture_data, std::string> create_default_texture(SDL_GPUDevice* device);

/// Release texture resources
void release_texture(SDL_GPUDevice* device, texture_data& tex);

} // namespace as3

