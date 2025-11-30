#pragma once

/// @file engine_settings_interface.hpp
/// @brief Interface for runtime engine settings accessible from game code

#include "window_settings.hpp"

#include <cstdint>
#include <string_view>

namespace as3
{

/// Interface for engine settings that can be modified at runtime
class IEngineSettings
{
public:
    virtual ~IEngineSettings() = default;

    // VSync control
    virtual void                     set_vsync(vsync_mode mode) noexcept = 0;
    [[nodiscard]] virtual vsync_mode get_vsync() const noexcept          = 0;

    // Fullscreen control
    virtual void               set_fullscreen(bool fullscreen) noexcept = 0;
    [[nodiscard]] virtual bool is_fullscreen() const noexcept           = 0;

    // Window info
    [[nodiscard]] virtual std::int32_t get_window_width() const noexcept  = 0;
    [[nodiscard]] virtual std::int32_t get_window_height() const noexcept = 0;

    // GPU info
    [[nodiscard]] virtual std::string_view get_gpu_driver() const noexcept = 0;

    // Frame timing
    [[nodiscard]] virtual float get_target_fps() const noexcept    = 0;
    virtual void                set_target_fps(float fps) noexcept = 0;

    // Mouse capture
    virtual void               set_mouse_captured(bool captured) noexcept = 0;
    [[nodiscard]] virtual bool is_mouse_captured() const noexcept         = 0;

    // Request quit
    virtual void request_quit() noexcept = 0;
};

} // namespace as3
