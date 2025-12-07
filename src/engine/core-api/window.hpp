#pragma once

#include <cstdint>
#include <string>

namespace euengine
{

enum class vsync_mode : std::uint8_t
{
    disabled = 0, ///< No VSync - uncapped framerate
    enabled  = 1, ///< Standard VSync - cap to monitor refresh
    adaptive = 2, ///< Adaptive VSync - allow tearing when below refresh
};

enum class window_mode : std::uint8_t
{
    windowed,           ///< Normal windowed mode
    borderless,         ///< Borderless windowed (fake fullscreen)
    fullscreen,         ///< Exclusive fullscreen
    fullscreen_desktop, ///< Fullscreen at desktop resolution
};

enum class msaa_samples : std::uint8_t
{
    none = 1,
    x2   = 2,
    x4   = 4,
    x8   = 8,
    x16  = 16,
};

struct window_settings final
{
    std::string  title             = "euengine";
    std::int32_t width             = 1280;
    std::int32_t height            = 720;
    std::int32_t min_width         = 640;
    std::int32_t min_height        = 480;
    window_mode  mode              = window_mode::windowed;
    vsync_mode   vsync             = vsync_mode::enabled;
    msaa_samples msaa              = msaa_samples::none;
    bool         resizable         = true;
    bool         high_dpi          = true;
    bool         allow_screensaver = false;

    [[nodiscard]] static constexpr window_settings windowed(
        std::int32_t w = 1280, std::int32_t h = 720) noexcept
    {
        return window_settings {
            .width  = w,
            .height = h,
            .mode   = window_mode::windowed,
        };
    }

    [[nodiscard]] static constexpr window_settings fullscreen() noexcept
    {
        return window_settings {
            .mode = window_mode::fullscreen_desktop,
        };
    }

    [[nodiscard]] static constexpr window_settings borderless() noexcept
    {
        return window_settings {
            .mode = window_mode::borderless,
        };
    }
};

struct renderer_settings final
{
    bool  wireframe_mode  = false;
    bool  show_debug_info = false;
    bool  frustum_culling = true;
    float render_scale    = 1.0f; ///< Internal resolution multiplier
    float max_anisotropy  = 16.0f;
    float gamma           = 2.2f;
    float exposure        = 1.0f;

    [[nodiscard]] static constexpr renderer_settings defaults() noexcept
    {
        return renderer_settings {};
    }
};

struct audio_settings final
{
    float music_volume  = 0.7f;
    float sound_volume  = 1.0f;
    float master_volume = 1.0f;
    bool  muted         = false;

    [[nodiscard]] static constexpr audio_settings defaults() noexcept
    {
        return audio_settings {};
    }
};

} // namespace euengine
