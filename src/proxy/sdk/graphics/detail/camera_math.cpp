#include "sdk/graphics/detail/camera_math.hpp"

#include <algorithm>
#include <cmath>

namespace sdk::graphics::detail
{

namespace
{

inline constexpr double k_minimum_length     = 1.0e-9;
inline constexpr double k_orthogonal_epsilon = 1.0e-3;

[[nodiscard]] bool finite(vec3 value) noexcept
{
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z);
}

[[nodiscard]] camera_pose pose_from_forward(vec3 eye, vec3 forward) noexcept
{
    return {
        .position    = eye,
        .yaw_degrees = std::atan2(forward.z, forward.x) * radians_to_degrees,
        .pitch_degrees =
            std::asin(std::clamp(forward.y, -1.0, 1.0)) * radians_to_degrees,
    };
}

} // namespace

std::optional<vec3> normalize(vec3 value) noexcept
{
    if (!finite(value))
    {
        return std::nullopt;
    }

    const auto squared_length = dot(value, value);
    if (!std::isfinite(squared_length) ||
        squared_length <= k_minimum_length * k_minimum_length)
    {
        return std::nullopt;
    }

    return scale(value, 1.0 / std::sqrt(squared_length));
}

camera_basis basis_from_pose(const camera_pose& pose) noexcept
{
    const auto yaw   = pose.yaw_degrees * degrees_to_radians;
    const auto pitch = pose.pitch_degrees * degrees_to_radians;
    const auto cp    = std::cos(pitch);
    const auto forward =
        normalize({ std::cos(yaw) * cp, std::sin(pitch), std::sin(yaw) * cp })
            .value_or(vec3{ 0.0, 0.0, -1.0 });
    const auto right =
        normalize(cross(forward, world_up)).value_or(vec3{ 1.0, 0.0, 0.0 });
    const auto up = normalize(cross(right, forward)).value_or(world_up);
    return { .forward = forward, .right = right, .up = up };
}

std::array<float, 16> make_right_handed_view(const camera_pose& pose) noexcept
{
    const auto basis  = basis_from_pose(pose);
    const auto z_axis = scale(basis.forward, -1.0);
    const auto x_axis =
        normalize(cross(world_up, z_axis)).value_or(basis.right);
    const auto y_axis = cross(z_axis, x_axis);
    const auto eye    = pose.position;

    return {
        static_cast<float>(x_axis.x),
        static_cast<float>(y_axis.x),
        static_cast<float>(z_axis.x),
        0.0F,
        static_cast<float>(x_axis.y),
        static_cast<float>(y_axis.y),
        static_cast<float>(z_axis.y),
        0.0F,
        static_cast<float>(x_axis.z),
        static_cast<float>(y_axis.z),
        static_cast<float>(z_axis.z),
        0.0F,
        static_cast<float>(-dot(x_axis, eye)),
        static_cast<float>(-dot(y_axis, eye)),
        static_cast<float>(-dot(z_axis, eye)),
        1.0F,
    };
}

std::optional<camera_pose> decompose_right_handed_view(
    std::span<const float, 16> matrix) noexcept
{
    for (const auto component : matrix)
    {
        if (!std::isfinite(component))
        {
            return std::nullopt;
        }
    }

    const auto x_axis = normalize({ matrix[0], matrix[4], matrix[8] });
    const auto y_axis = normalize({ matrix[1], matrix[5], matrix[9] });
    const auto z_axis = normalize({ matrix[2], matrix[6], matrix[10] });
    if (!x_axis || !y_axis || !z_axis)
    {
        return std::nullopt;
    }

    if (std::abs(dot(*x_axis, *y_axis)) > k_orthogonal_epsilon ||
        std::abs(dot(*x_axis, *z_axis)) > k_orthogonal_epsilon ||
        std::abs(dot(*y_axis, *z_axis)) > k_orthogonal_epsilon)
    {
        return std::nullopt;
    }

    const vec3 translation{ matrix[12], matrix[13], matrix[14] };
    const auto eye =
        add(add(scale(*x_axis, -translation.x), scale(*y_axis, -translation.y)),
            scale(*z_axis, -translation.z));
    const auto forward = scale(*z_axis, -1.0);
    if (!finite(eye))
    {
        return std::nullopt;
    }
    return pose_from_forward(eye, forward);
}

std::optional<camera_pose> pose_from_look_at(vec3 eye, vec3 center) noexcept
{
    const auto forward = normalize(subtract(center, eye));
    if (!finite(eye) || !forward)
    {
        return std::nullopt;
    }
    return pose_from_forward(eye, *forward);
}

} // namespace sdk::graphics::detail
