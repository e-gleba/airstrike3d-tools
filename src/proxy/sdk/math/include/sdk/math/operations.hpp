#pragma once

// sdk/math/operations.hpp — Math utility functions with concept constraints
//
// Stefano-style: heavy use of concepts for type safety.

#include "sdk/math/types.hpp"

#include <concepts>
#include <type_traits>

namespace sdk::math
{

// ─── Concept-constrained conversions ──────────────────────────────────────

template <typename T>
concept convertible_to_double = std::convertible_to<T, double>;

template <convertible_to_double T>
[[nodiscard]] constexpr auto to_radians(T degrees) noexcept -> double
{
    return static_cast<double>(degrees) * (3.14159265358979323846 / 180.0);
}

template <convertible_to_double T>
[[nodiscard]] constexpr auto to_degrees(T radians) noexcept -> double
{
    return static_cast<double>(radians) * (180.0 / 3.14159265358979323846);
}

// ─── Concept-constrained clamping ─────────────────────────────────────────

template <numeric T>
[[nodiscard]] constexpr auto clamp_value(T value, T min, T max) noexcept -> T
{
    return (value < min) ? min : (value > max) ? max : value;
}

// ─── Modular arithmetic ───────────────────────────────────────────────────

[[nodiscard]] inline auto fmod_safe(double value, double divisor) noexcept -> double
{
    return (divisor != 0.0) ? mod(value, divisor) : 0.0;
}

// ─── Angle normalization ─────────────────────────────────────────────────

[[nodiscard]] inline auto normalize_angle(double radians) noexcept -> double
{
    constexpr double two_pi = 2.0 * 3.14159265358979323846;
    auto result = fmod_safe(radians, two_pi);
    if (result < 0.0)
    {
        result += two_pi;
    }
    return result;
}

} // namespace sdk::math
