#pragma once

// sdk/math/vec3.hpp — Vector3 operations with C++23 concepts
//
// Stefano-style: concepts constrain operations to valid types.
// Turner-style: constexpr where possible.

#include "sdk/math/types.hpp"

#include <concepts>

namespace sdk::math
{

// ─── Distance and projection ───────────────────────────────────────────────

[[nodiscard]] inline auto distance(vec3 const& a, vec3 const& b) noexcept -> double
{
    return (b - a).length();
}

[[nodiscard]] constexpr auto dot_product(vec3 const& a, vec3 const& b) noexcept -> double
{
    return a.dot(b);
}

[[nodiscard]] constexpr auto cross_product(vec3 const& a, vec3 const& b) noexcept -> vec3
{
    return a.cross(b);
}

// ─── Linear interpolation ─────────────────────────────────────────────────

[[nodiscard]] constexpr auto lerp(vec3 const& a, vec3 const& b, double t) noexcept -> vec3
{
    return a + (b - a) * t;
}

// ─── Concept-constrained projection ───────────────────────────────────────

template <vec3_like V>
[[nodiscard]] auto project_onto(V const& v, V const& onto) noexcept -> vec3
{
    auto v_vec    = vec3{ static_cast<double>(v.x), static_cast<double>(v.y), static_cast<double>(v.z) };
    auto onto_vec = vec3{ static_cast<double>(onto.x), static_cast<double>(onto.y), static_cast<double>(onto.z) };
    
    auto len_sq = onto_vec.dot(onto_vec);
    if (len_sq <= 0.0)
    {
        return vec3{};
    }
    
    return onto_vec * (v_vec.dot(onto_vec) / len_sq);
}

} // namespace sdk::math
