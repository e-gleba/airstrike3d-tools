/// @file camera_math.hpp
/// @brief Renderer-neutral right-handed camera math.

#pragma once

#include "sdk/graphics/rendering.hpp"

#include <array>
#include <cmath>
#include <optional>
#include <span>

namespace sdk::graphics::detail
{

inline constexpr double pi                 = 3.14159265358979323846;
inline constexpr double degrees_to_radians = pi / 180.0;
inline constexpr double radians_to_degrees = 180.0 / pi;
inline constexpr vec3   world_up{ 0.0, 1.0, 0.0 };
inline constexpr vec3   world_right{ 1.0, 0.0, 0.0 };

struct camera_basis final
{
    vec3 forward{};
    vec3 right{};
    vec3 up{};
};

[[nodiscard]] constexpr vec3 add(vec3 lhs, vec3 rhs) noexcept
{
    return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z };
}

[[nodiscard]] constexpr vec3 subtract(vec3 lhs, vec3 rhs) noexcept
{
    return { lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z };
}

[[nodiscard]] constexpr vec3 scale(vec3 value, double factor) noexcept
{
    return { value.x * factor, value.y * factor, value.z * factor };
}

[[nodiscard]] constexpr double dot(vec3 lhs, vec3 rhs) noexcept
{
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

[[nodiscard]] constexpr vec3 cross(vec3 lhs, vec3 rhs) noexcept
{
    return {
        lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.z * rhs.x - lhs.x * rhs.z,
        lhs.x * rhs.y - lhs.y * rhs.x,
    };
}

[[nodiscard]] constexpr bool is_finite(vec3 value) noexcept
{
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z);
}

[[nodiscard]] constexpr bool is_finite(const camera_pose& pose) noexcept
{
    return is_finite(pose.position) && std::isfinite(pose.yaw_degrees) &&
           std::isfinite(pose.pitch_degrees);
}

[[nodiscard]] std::optional<vec3> normalize(vec3 value) noexcept;

/// @throws std::invalid_argument if @p pose is non-finite or degenerate.
[[nodiscard]] camera_basis basis_from_pose(const camera_pose& pose);

/// @throws std::invalid_argument if @p pose is non-finite or degenerate.
[[nodiscard]] std::array<float, 16> make_right_handed_view(
    const camera_pose& pose);

[[nodiscard]] std::optional<camera_pose> decompose_right_handed_view(
    std::span<const float, 16> matrix) noexcept;

[[nodiscard]] std::optional<camera_pose> pose_from_look_at(
    vec3 eye, vec3 center) noexcept;

} // namespace sdk::graphics::detail
