/// @file math.hpp
/// @brief Math utilities for scripting bindings (standard-library types only).

#pragma once

#include "sdk/core/types.hpp"

namespace sdk::math
{

[[nodiscard]] double radians(double degrees) noexcept;
[[nodiscard]] double cos(double v) noexcept;
[[nodiscard]] double sin(double v) noexcept;

/// @throws std::invalid_argument if @p divisor is zero.
[[nodiscard]] double mod(double v, double divisor);

/// @throws std::invalid_argument if @p lo exceeds @p hi.
[[nodiscard]] double clamp(double v, double lo, double hi);

/// @throws std::invalid_argument if the input vector has zero length.
[[nodiscard]] vec3 normalize(double x, double y, double z);

[[nodiscard]] vec3 cross(double ax, double ay, double az,
                         double bx, double by, double bz) noexcept;

[[nodiscard]] mat4 lookat_matrix(double ex, double ey, double ez,
                                 double cx, double cy, double cz,
                                 double ux, double uy, double uz) noexcept;

} // namespace sdk::math
