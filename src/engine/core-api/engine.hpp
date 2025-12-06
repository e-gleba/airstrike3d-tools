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

    virtual void                    set_msaa(msaa_samples samples) noexcept = 0;
    [[nodiscard]] virtual msaa_samples get_msaa() const noexcept            = 0;

    virtual void                set_render_scale(float scale) noexcept = 0;
    [[nodiscard]] virtual float get_render_scale() const noexcept     = 0;

    virtual void                set_max_anisotropy(float anisotropy) noexcept = 0;
    [[nodiscard]] virtual float get_max_anisotropy() const noexcept          = 0;

    virtual void request_quit() noexcept = 0;

    virtual void stop() noexcept = 0;

    /// Reload the game module (hot-reload)
    [[nodiscard]] virtual bool reload_game() noexcept = 0;
};

} // namespace euengine
