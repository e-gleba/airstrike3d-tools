/// @file rendering.hpp
/// @brief Renderer-neutral graphics controls exposed to scripts.

#pragma once

#include "sdk/core/types.hpp"

#include <cstdint>
#include <memory>
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

struct line_settings final
{
    bool depth_test{};
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

void               set_camera_enabled(bool enabled) noexcept;
[[nodiscard]] bool camera_enabled() noexcept;

/// @throws std::invalid_argument if @p pose is non-finite.
void                      set_camera_pose(camera_pose pose);
[[nodiscard]] camera_pose get_camera_pose() noexcept;
[[nodiscard]] bool        adopt_observed_camera() noexcept;
[[nodiscard]] bool        has_observed_camera() noexcept;

/// @throws std::invalid_argument if any delta is non-finite or the pose is
/// degenerate.
void move_camera_local(double forward, double right, double up);

/// @throws std::invalid_argument if any delta is non-finite.
void rotate_camera(double yaw_delta_degrees, double pitch_delta_degrees);

/// @throws std::invalid_argument if @p vertices are malformed.
void set_world_lines(std::span<const line_vertex> vertices,
                     line_settings                settings = {});
void clear_world_lines() noexcept;

void set_visual_settings(visual_settings settings) noexcept;
[[nodiscard]] visual_settings get_visual_settings() noexcept;

namespace detail
{

struct world_line_batch final
{
    std::vector<line_vertex> vertices;
    line_settings            settings;
    std::uint64_t            generation{};
};

void set_active_backend(render_api backend) noexcept;

/// @throws std::invalid_argument if @p pose is non-finite.
void observe_camera(camera_pose pose);

[[nodiscard]] auto world_lines_snapshot() noexcept
    -> std::shared_ptr<const world_line_batch>;

} // namespace detail

} // namespace sdk::graphics
