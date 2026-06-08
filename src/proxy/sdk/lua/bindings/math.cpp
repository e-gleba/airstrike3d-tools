/// @file math.cpp
/// @brief Lua bindings for mathematical operations (vectors, matrices).

#include "bindings.hpp"

#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <ranges>
#include <span>
#include <tuple>

namespace sdk::lua::bindings
{

namespace
{

/// @brief Convert glm::dvec3 to Lua-friendly tuple.
[[nodiscard]] constexpr auto to_tuple(const glm::dvec3& v) noexcept
    -> std::tuple<double, double, double>
{
    return {v.x, v.y, v.z};
}

/// @brief Create glm::dvec3 from components.
[[nodiscard]] constexpr auto vec3(const double x, const double y, const double z) noexcept
    -> glm::dvec3
{
    return {x, y, z};
}

/// @brief Convert glm::dmat4 to Lua table (1-indexed, column-major).
[[nodiscard]] auto mat4_to_table(sol::state& state, const glm::dmat4& mat) -> sol::table
{
    auto tbl{state.create_table(16, 0)};
    const auto ptr{std::span(glm::value_ptr(mat), 16)};

    for (const auto i : std::views::iota(0uz, 16uz))
    {
        tbl[i + 1] = ptr[i]; // Lua is 1-indexed
    }

    return tbl;
}

} // namespace

void register_math(sol::state& state)
{
    auto gmath{state.create_named_table("gmath")};

    // Trigonometric functions
    gmath.set_function("radians", [](const double d) noexcept { return glm::radians(d); });
    gmath.set_function("cos", [](const double v) noexcept { return std::cos(v); });
    gmath.set_function("sin", [](const double v) noexcept { return std::sin(v); });

    // Utility functions
    gmath.set_function("mod", [](const double v, const double d) noexcept { return glm::mod(v, d); });
    gmath.set_function("clamp", [](const double v, const double lo, const double hi) noexcept
                       { return glm::clamp(v, lo, hi); });

    // Vector operations
    gmath.set_function("normalize",
                       [](const double x, const double y, const double z) noexcept
                       { return to_tuple(glm::normalize(vec3(x, y, z))); });

    gmath.set_function("cross",
                       [](const double ax, const double ay, const double az,
                          const double bx, const double by, const double bz) noexcept
                       { return to_tuple(glm::cross(vec3(ax, ay, az), vec3(bx, by, bz))); });

    // Matrix operations
    gmath.set_function("lookat_matrix",
                       [&state](const double ex, const double ey, const double ez,
                                const double cx, const double cy, const double cz,
                                const double ux, const double uy, const double uz) -> sol::table
                       {
                           return mat4_to_table(state,
                                                glm::lookAt(vec3(ex, ey, ez),
                                                            vec3(cx, cy, cz),
                                                            vec3(ux, uy, uz)));
                       });
}

} // namespace sdk::lua::bindings
