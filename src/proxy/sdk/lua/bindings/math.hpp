/// @file bindings/math.hpp
/// @brief Pure C++ math functions for Lua binding (no sol2 types).

#pragma once

#include <glm/glm.hpp>
#include <tuple>

namespace sdk::lua::bindings::math
{

[[nodiscard]] double radians(double degrees) noexcept;
[[nodiscard]] double cos(double v) noexcept;
[[nodiscard]] double sin(double v) noexcept;
[[nodiscard]] double mod(double v, double d) noexcept;
[[nodiscard]] double clamp(double v, double lo, double hi) noexcept;

[[nodiscard]] glm::dvec3 normalize(double x, double y, double z) noexcept;
[[nodiscard]] glm::dvec3 cross(double ax, double ay, double az,
                                double bx, double by, double bz) noexcept;
[[nodiscard]] glm::dmat4 lookat_matrix(double ex, double ey, double ez,
                                        double cx, double cy, double cz,
                                        double ux, double uy, double uz) noexcept;

} // namespace sdk::lua::bindings::math
