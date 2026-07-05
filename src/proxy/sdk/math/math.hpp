/// @file math.hpp
/// @brief Math utilities for scripting bindings (standard-library types only).

#pragma once

#include "sdk/core/types.hpp"

namespace sdk::math
{

[[nodiscard]] double radians(double degrees);
[[nodiscard]] double cos(double v);
[[nodiscard]] double sin(double v);
[[nodiscard]] double mod(double v, double d);
[[nodiscard]] double clamp(double v, double lo, double hi);

[[nodiscard]] vec3 normalize(double x, double y, double z);
[[nodiscard]] vec3 cross(double ax, double ay, double az,
                         double bx, double by, double bz);
[[nodiscard]] mat4 lookat_matrix(double ex, double ey, double ez,
                                 double cx, double cy, double cz,
                                 double ux, double uy, double uz);

} // namespace sdk::math
