/// @file rendering.hpp
/// @brief Renderer-neutral graphics controls exposed to scripts.

#pragma once

#include "sdk/core/types.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace sdk::graphics
{

enum class capability : std::uint8_t
{
    overlay,
    free_camera,
    world_lines,
    visual_override
};

struct camera_pose final
{
    vec3   position{};
    double yaw_degrees{ -90.0 };
    double pitch_degrees{};
};

struct line_vertex final
{
    float         x{};
    float         y{};
    float         z{};
    std::uint32_t argb{ 0xFFFFFFFFU };
};

enum class visual_mode : std::uint8_t
{
    disabled,
    xray,
    wireframe,
    ghost,
    depth_bias
};

struct visual_settings final
{
    visual_mode   mode{ visual_mode::disabled };
    float         alpha{ 0.3F };
    float         depth_bias{ -0.05F };
    std::uint32_t argb{ 0xB200FF00U };
};

[[nodiscard]] render_api active_backend() noexcept;
[[nodiscard]] bool       supports(capability value) noexcept;

void                      set_camera_enabled(bool enabled) noexcept;
[[nodiscard]] bool        camera_enabled() noexcept;
void                      set_camera_pose(camera_pose pose) noexcept;
[[nodiscard]] camera_pose get_camera_pose() noexcept;

void set_world_lines(std::span<const line_vertex> vertices);
void clear_world_lines() noexcept;

void set_visual_settings(visual_settings settings) noexcept;
[[nodiscard]] visual_settings get_visual_settings() noexcept;

namespace detail
{

void               set_active_backend(render_api backend) noexcept;
[[nodiscard]] auto world_lines_snapshot() -> std::vector<line_vertex>;

} // namespace detail

} // namespace sdk::graphics
