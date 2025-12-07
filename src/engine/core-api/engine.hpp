#pragma once

#include "window.hpp"

#include <cstdint>
#include <string_view>

namespace euengine
{

class i_engine_settings
{
public:
    virtual ~i_engine_settings() = default;

    virtual void                     set_vsync(vsync_mode mode) noexcept = 0;
    [[nodiscard]] virtual vsync_mode get_vsync() const noexcept          = 0;

    virtual void               set_fullscreen(bool fullscreen) noexcept = 0;
    [[nodiscard]] virtual bool is_fullscreen() const noexcept           = 0;

    [[nodiscard]] virtual std::int32_t get_window_width() const noexcept  = 0;
    [[nodiscard]] virtual std::int32_t get_window_height() const noexcept = 0;

    [[nodiscard]] virtual std::string_view get_gpu_driver() const noexcept = 0;

    [[nodiscard]] virtual float get_target_fps() const noexcept    = 0;
    virtual void                set_target_fps(float fps) noexcept = 0;

    virtual void               set_mouse_captured(bool captured) noexcept = 0;
    [[nodiscard]] virtual bool is_mouse_captured() const noexcept         = 0;

    virtual void                set_master_volume(float volume) noexcept = 0;
    [[nodiscard]] virtual float get_master_volume() const noexcept       = 0;
    [[nodiscard]] virtual bool  is_audio_available() const noexcept      = 0;

    virtual void set_msaa(msaa_samples samples) noexcept         = 0;
    [[nodiscard]] virtual msaa_samples get_msaa() const noexcept = 0;

    virtual void                set_render_scale(float scale) noexcept = 0;
    [[nodiscard]] virtual float get_render_scale() const noexcept      = 0;

    virtual void set_max_anisotropy(float anisotropy) noexcept      = 0;
    [[nodiscard]] virtual float get_max_anisotropy() const noexcept = 0;

    /// Frame buffering (1-3, lower = less latency, higher = smoother)
    virtual void set_frames_in_flight(std::uint32_t frames) noexcept = 0;
    [[nodiscard]] virtual std::uint32_t get_frames_in_flight()
        const noexcept = 0;

    /// Check if MSAA sample count is supported by GPU
    [[nodiscard]] virtual bool is_msaa_supported(
        msaa_samples samples) const noexcept = 0;

    /// FXAA (Fast Approximate Anti-Aliasing) - post-processing
    virtual void               set_fxaa_enabled(bool enabled) noexcept = 0;
    [[nodiscard]] virtual bool is_fxaa_enabled() const noexcept        = 0;

    /// Texture filtering quality
    enum class texture_filter : std::uint8_t
    {
        nearest   = 0, ///< Nearest neighbor (pixelated)
        linear    = 1, ///< Bilinear filtering
        trilinear = 2, ///< Trilinear with mipmaps
    };
    virtual void set_texture_filter(texture_filter filter) noexcept = 0;
    [[nodiscard]] virtual texture_filter get_texture_filter()
        const noexcept = 0;

    /// Gamma correction (1.0-3.0, default 2.2)
    virtual void                set_gamma(float gamma) noexcept = 0;
    [[nodiscard]] virtual float get_gamma() const noexcept      = 0;

    /// Brightness adjustment (-1.0 to 1.0, default 0.0)
    virtual void                set_brightness(float brightness) noexcept = 0;
    [[nodiscard]] virtual float get_brightness() const noexcept           = 0;

    /// Contrast adjustment (0.5 to 2.0, default 1.0)
    virtual void                set_contrast(float contrast) noexcept = 0;
    [[nodiscard]] virtual float get_contrast() const noexcept         = 0;

    /// Saturation adjustment (0.0 to 2.0, default 1.0)
    virtual void                set_saturation(float saturation) noexcept = 0;
    [[nodiscard]] virtual float get_saturation() const noexcept           = 0;

    /// Vignette intensity (0.0 to 1.0, default 0.0 = off)
    virtual void                set_vignette(float intensity) noexcept = 0;
    [[nodiscard]] virtual float get_vignette() const noexcept          = 0;

    /// Render distance / Far plane (10.0 to 10000.0, default 200.0)
    virtual void set_render_distance(float distance) noexcept        = 0;
    [[nodiscard]] virtual float get_render_distance() const noexcept = 0;

    virtual void request_quit() noexcept = 0;

    virtual void stop() noexcept = 0;

    /// Reload the game module (hot-reload)
    [[nodiscard]] virtual bool reload_game() noexcept = 0;
};

} // namespace euengine
