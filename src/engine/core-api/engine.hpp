#pragma once

#include "window.hpp"

#include <cstdint>
#include <string_view>

namespace as3
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

    virtual void request_quit() noexcept = 0;

    virtual void stop() noexcept = 0;
};

} // namespace as3