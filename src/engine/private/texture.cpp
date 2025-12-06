#include "texture.hpp"

#include <stb_image.h>

#include <cstring>
#include <format>

namespace euengine
{

std::expected<texture_data, std::string> load_texture(
    SDL_GPUDevice*               device,
    const std::filesystem::path& path,
    bool                         flip_vertical)
{
    if (device == nullptr)
    {
        return std::unexpected("null device");
    }
    if (path.empty() || !std::filesystem::exists(path))
    {
        return std::unexpected(
            std::format("file not found: {}", path.string()));
    }

    std::int32_t w  = 0;
    std::int32_t h  = 0;
    std::int32_t ch = 0;

    stbi_set_flip_vertically_on_load(flip_vertical ? 1 : 0);
    auto* pixels = stbi_load(path.c_str(), &w, &h, &ch, 4);
    if (pixels == nullptr)
    {
        return std::unexpected(
            std::format("stbi_load failed: {}", stbi_failure_reason()));
    }

    // Create GPU texture
    SDL_GPUTextureCreateInfo tex_info {};
    tex_info.type                 = SDL_GPU_TEXTURETYPE_2D;
    tex_info.format               = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    tex_info.usage                = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    tex_info.width                = static_cast<Uint32>(w);
    tex_info.height               = static_cast<Uint32>(h);
    tex_info.layer_count_or_depth = 1;
    tex_info.num_levels           = 1;
    tex_info.sample_count         = SDL_GPU_SAMPLECOUNT_1;

    auto* tex = SDL_CreateGPUTexture(device, &tex_info);
    if (tex == nullptr)
    {
        stbi_image_free(pixels);
        return std::unexpected(
            std::format("SDL_CreateGPUTexture: {}", SDL_GetError()));
    }

    // Create sampler with linear filtering
    SDL_GPUSamplerCreateInfo samp_info {};
    samp_info.min_filter     = SDL_GPU_FILTER_LINEAR;
    samp_info.mag_filter     = SDL_GPU_FILTER_LINEAR;
    samp_info.mipmap_mode    = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
    samp_info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    samp_info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    samp_info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    auto* samp               = SDL_CreateGPUSampler(device, &samp_info);

    const auto data_size = static_cast<Uint32>(w * h * 4);

    // Create transfer buffer and upload pixel data
    SDL_GPUTransferBufferCreateInfo tb_info {};
    tb_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tb_info.size  = data_size;
    auto* tb      = SDL_CreateGPUTransferBuffer(device, &tb_info);
    auto* ptr     = SDL_MapGPUTransferBuffer(device, tb, false);
    std::memcpy(ptr, pixels, data_size);
    SDL_UnmapGPUTransferBuffer(device, tb);
    stbi_image_free(pixels);

    // Submit upload command
    auto* cmd = SDL_AcquireGPUCommandBuffer(device);
    auto* cp  = SDL_BeginGPUCopyPass(cmd);

    SDL_GPUTextureTransferInfo src {};
    src.transfer_buffer = tb;
    src.offset          = 0;
    SDL_GPUTextureRegion dst {};
    dst.texture = tex;
    dst.w       = static_cast<Uint32>(w);
    dst.h       = static_cast<Uint32>(h);
    dst.d       = 1;
    SDL_UploadToGPUTexture(cp, &src, &dst, false);

    SDL_EndGPUCopyPass(cp);
    SDL_SubmitGPUCommandBuffer(cmd);
    SDL_ReleaseGPUTransferBuffer(device, tb);

    return texture_data {
        .texture = tex, .sampler = samp, .width = w, .height = h
    };
}

std::expected<texture_data, std::string> create_default_texture(
    SDL_GPUDevice* device)
{
    if (device == nullptr)
    {
        return std::unexpected("null device");
    }

    constexpr std::uint32_t white = 0xFFFFFFFF;

    SDL_GPUTextureCreateInfo tex_info {};
    tex_info.type                 = SDL_GPU_TEXTURETYPE_2D;
    tex_info.format               = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    tex_info.usage                = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    tex_info.width                = 1;
    tex_info.height               = 1;
    tex_info.layer_count_or_depth = 1;
    tex_info.num_levels           = 1;
    tex_info.sample_count         = SDL_GPU_SAMPLECOUNT_1;

    auto* tex = SDL_CreateGPUTexture(device, &tex_info);
    if (tex == nullptr)
    {
        return std::unexpected(
            std::format("SDL_CreateGPUTexture: {}", SDL_GetError()));
    }

    SDL_GPUSamplerCreateInfo samp_info {};
    samp_info.min_filter     = SDL_GPU_FILTER_NEAREST;
    samp_info.mag_filter     = SDL_GPU_FILTER_NEAREST;
    samp_info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    samp_info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    samp_info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    auto* samp               = SDL_CreateGPUSampler(device, &samp_info);

    // Upload single white pixel
    SDL_GPUTransferBufferCreateInfo tb_info {};
    tb_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tb_info.size  = 4;
    auto* tb      = SDL_CreateGPUTransferBuffer(device, &tb_info);
    auto* ptr     = SDL_MapGPUTransferBuffer(device, tb, false);
    std::memcpy(ptr, &white, 4);
    SDL_UnmapGPUTransferBuffer(device, tb);

    auto* cmd = SDL_AcquireGPUCommandBuffer(device);
    auto* cp  = SDL_BeginGPUCopyPass(cmd);

    SDL_GPUTextureTransferInfo src {};
    src.transfer_buffer = tb;
    src.offset          = 0;
    SDL_GPUTextureRegion dst {};
    dst.texture = tex;
    dst.w       = 1;
    dst.h       = 1;
    dst.d       = 1;
    SDL_UploadToGPUTexture(cp, &src, &dst, false);

    SDL_EndGPUCopyPass(cp);
    SDL_SubmitGPUCommandBuffer(cmd);
    SDL_ReleaseGPUTransferBuffer(device, tb);

    return texture_data {
        .texture = tex, .sampler = samp, .width = 1, .height = 1
    };
}

void release_texture(SDL_GPUDevice* device, texture_data& tex)
{
    if (tex.texture != nullptr)
    {
        SDL_ReleaseGPUTexture(device, tex.texture);
    }
    if (tex.sampler != nullptr)
    {
        SDL_ReleaseGPUSampler(device, tex.sampler);
    }
    tex = {};
}

} // namespace euengine
