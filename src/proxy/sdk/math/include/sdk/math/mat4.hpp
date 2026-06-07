#pragma once

// sdk/math/mat4.hpp — Matrix4 operations with C++23 concepts
//
// Column-major layout (OpenGL convention).
// Turner-style: constexpr identity, noexcept operations.

#include "sdk/math/types.hpp"

#include <concepts>

namespace sdk::math
{

// ─── Matrix construction ──────────────────────────────────────────────────

[[nodiscard]] auto look_at(vec3 const& eye, vec3 const& center, vec3 const& up) noexcept -> mat4;

[[nodiscard]] auto perspective(double fov_radians, double aspect, double near, double far) noexcept -> mat4;

[[nodiscard]] auto ortho(double left, double right, double bottom, double top, double near, double far) noexcept -> mat4;

// ─── Matrix operations ────────────────────────────────────────────────────

[[nodiscard]] auto multiply(mat4 const& a, mat4 const& b) noexcept -> mat4;

[[nodiscard]] auto transpose(mat4 const& m) noexcept -> mat4;

[[nodiscard]] auto inverse(mat4 const& m) noexcept -> mat4;

// ─── Transform application ────────────────────────────────────────────────

[[nodiscard]] auto transform_point(mat4 const& m, vec3 const& p) noexcept -> vec3;

[[nodiscard]] auto transform_direction(mat4 const& m, vec3 const& d) noexcept -> vec3;

} // namespace sdk::math
