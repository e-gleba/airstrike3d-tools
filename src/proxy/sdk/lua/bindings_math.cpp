#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <sol/sol.hpp>

#include <ranges>
#include <span>
#include <tuple>

namespace sdk::lua
{

namespace
{

[[nodiscard]] constexpr auto to_tuple(const glm::dvec3& v) noexcept
    -> std::tuple<double, double, double>
{
    return { v.x, v.y, v.z };
}

[[nodiscard]] constexpr auto vec3(double x, double y, double z) noexcept
    -> glm::dvec3
{
    return { x, y, z };
}

[[nodiscard]] auto mat4_to_table(sol::state& state, const glm::dmat4& mat)
    -> sol::table
{
    auto t{ state.create_table(16, 0) };
    auto p{ std::span(glm::value_ptr(mat), 16) };

    for (auto i : std::views::iota(0uz, 16uz))
    {
        t[i + 1] = p[i];
    }

    return t;
}

} // namespace

void register_math_bindings(sol::state& sol_state)
{
    auto m{ sol_state.create_named_table("gmath") };

    m.set_function("radians",
                   [](double d) noexcept { return glm::radians(d); });
    m.set_function("cos", [](double v) noexcept { return std::cos(v); });
    m.set_function("sin", [](double v) noexcept { return std::sin(v); });
    m.set_function("mod",
                   [](double v, double d) noexcept { return glm::mod(v, d); });

    m.set_function("clamp",
                   [](double v, double lo, double hi) noexcept
                   { return glm::clamp(v, lo, hi); });

    m.set_function("normalize",
                   [](double x, double y, double z) noexcept
                   { return to_tuple(glm::normalize(vec3(x, y, z))); });

    m.set_function(
        "cross",
        [](double ax,
           double ay,
           double az,
           double bx,
           double by,
           double bz) noexcept
        { return to_tuple(glm::cross(vec3(ax, ay, az), vec3(bx, by, bz))); });

    m.set_function("lookat_matrix",
                   [&sol_state](double ex,
                                double ey,
                                double ez,
                                double cx,
                                double cy,
                                double cz,
                                double ux,
                                double uy,
                                double uz) -> sol::table
                   {
                       return mat4_to_table(sol_state,
                                            glm::lookAt(vec3(ex, ey, ez),
                                                        vec3(cx, cy, cz),
                                                        vec3(ux, uy, uz)));
                   });
}

} // namespace sdk::lua