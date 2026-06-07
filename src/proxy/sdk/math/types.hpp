#pragma once

// sdk/math/types.hpp — Pure C++23 math types (no GLM, no external deps)
//
// Design: Polukhin-style ABI-stable types, Turner-style constexpr everywhere,
//         Stefano-style concept constraints.

#include <array>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <ostream>

namespace sdk::math
{

// ─── Concepts ──────────────────────────────────────────────────────────────

template <typename T>
concept numeric = std::integral<T> || std::floating_point<T>;

template <typename T>
concept vec3_like = requires(T v) {
    { v.x } -> std::convertible_to<double>;
    { v.y } -> std::convertible_to<double>;
    { v.z } -> std::convertible_to<double>;
};

// ─── vec3 ──────────────────────────────────────────────────────────────────

struct vec3
{
    double x{}, y{}, z{};

    [[nodiscard]] constexpr auto operator+(vec3 const& rhs) const noexcept -> vec3
    {
        return { x + rhs.x, y + rhs.y, z + rhs.z };
    }

    [[nodiscard]] constexpr auto operator-(vec3 const& rhs) const noexcept -> vec3
    {
        return { x - rhs.x, y - rhs.y, z - rhs.z };
    }

    [[nodiscard]] constexpr auto operator*(double s) const noexcept -> vec3
    {
        return { x * s, y * s, z * s };
    }

    [[nodiscard]] constexpr auto operator/(double s) const noexcept -> vec3
    {
        return { x / s, y / s, z / s };
    }

    [[nodiscard]] constexpr auto dot(vec3 const& rhs) const noexcept -> double
    {
        return x * rhs.x + y * rhs.y + z * rhs.z;
    }

    [[nodiscard]] constexpr auto cross(vec3 const& rhs) const noexcept -> vec3
    {
        return { y * rhs.z - z * rhs.y,
                 z * rhs.x - x * rhs.z,
                 x * rhs.y - y * rhs.x };
    }

    [[nodiscard]] auto length() const noexcept -> double
    {
        return std::sqrt(dot(*this));
    }

    [[nodiscard]] auto normalized() const noexcept -> vec3
    {
        auto len = length();
        return (len > 0.0) ? (*this / len) : vec3{};
    }

    [[nodiscard]] constexpr auto operator==(vec3 const&) const noexcept -> bool = default;

    friend auto operator<<(std::ostream& os, vec3 const& v) -> std::ostream&
    {
        return os << "vec3(" << v.x << ", " << v.y << ", " << v.z << ")";
    }
};

// ─── mat4 ──────────────────────────────────────────────────────────────────

struct mat4
{
    std::array<double, 16> m{}; // column-major, OpenGL convention

    [[nodiscard]] static constexpr auto identity() noexcept -> mat4
    {
        return { .m = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 } };
    }

    [[nodiscard]] constexpr auto at(std::size_t row, std::size_t col) const noexcept -> double
    {
        return m[col * 4 + row];
    }

    [[nodiscard]] constexpr auto operator[](std::size_t i) const noexcept -> double
    {
        return m[i];
    }

    [[nodiscard]] auto data() noexcept -> double*
    {
        return m.data();
    }

    [[nodiscard]] auto data() const noexcept -> double const*
    {
        return m.data();
    }

    [[nodiscard]] static constexpr auto size() noexcept -> std::size_t
    {
        return 16;
    }
};

// ─── Free functions ────────────────────────────────────────────────────────

[[nodiscard]] inline auto radians(double degrees) noexcept -> double
{
    return degrees * (3.14159265358979323846 / 180.0);
}

[[nodiscard]] inline auto clamp(double v, double lo, double hi) noexcept -> double
{
    return (v < lo) ? lo : (v > hi) ? hi : v;
}

[[nodiscard]] inline auto mod(double v, double d) noexcept -> double
{
    return std::fmod(v, d);
}

} // namespace sdk::math
