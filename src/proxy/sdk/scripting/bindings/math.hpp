/// @file bindings/math.hpp
/// @brief Mathematical functions for scripting — no backend types.

#pragma once

#include <array>

namespace sdk::scripting::bindings::math
{

/// Simple 3D vector (implementation detail hidden).
struct vec3
{
    double x, y, z;
};

/// Simple 4x4 matrix in column-major order (implementation detail hidden).
using mat4 = std::array<double, 16>;

/// Convert degrees to radians.
[[nodiscard]] double radians(double degrees) noexcept;

/// Cosine function.
[[nodiscard]] double cos(double v) noexcept;

/// Sine function.
[[nodiscard]] double sin(double v) noexcept;

/// Modulo operation.
[[nodiscard]] double mod(double v, double d) noexcept;

/// Clamp value to range.
[[nodiscard]] double clamp(double v, double lo, double hi) noexcept;

/// Normalize a 3D vector.
[[nodiscard]] vec3 normalize(double x, double y, double z) noexcept;

/// Cross product of two 3D vectors.
[[nodiscard]] vec3 cross(double ax, double ay, double az,
                         double bx, double by, double bz) noexcept;

/// Compute look-at matrix (column-major 4x4).
[[nodiscard]] mat4 lookat_matrix(double ex, double ey, double ez,
                                 double cx, double cy, double cz,
                                 double ux, double uy, double uz) noexcept;

} // namespace sdk::scripting::bindings::math
