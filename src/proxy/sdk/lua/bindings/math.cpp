// src/proxy/sdk/lua/bindings/math.cpp
// Mathematical functions exposed to Lua.
// Pure C++ — no sol2 types.

#include <cmath>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace sdk::lua::bindings::math
{

double radians(double degrees) noexcept
{
    return glm::radians(degrees);
}

double cos(double v) noexcept
{
    return std::cos(v);
}

double sin(double v) noexcept
{
    return std::sin(v);
}

double mod(double v, double d) noexcept
{
    return glm::mod(v, d);
}

double clamp(double v, double lo, double hi) noexcept
{
    return glm::clamp(v, lo, hi);
}

glm::dvec3 normalize(double x, double y, double z) noexcept
{
    return glm::normalize(glm::dvec3{x, y, z});
}

glm::dvec3 cross(double ax, double ay, double az, double bx, double by, double bz) noexcept
{
    return glm::cross(glm::dvec3{ax, ay, az}, glm::dvec3{bx, by, bz});
}

glm::dmat4 lookat_matrix(double ex, double ey, double ez,
                         double cx, double cy, double cz,
                         double ux, double uy, double uz) noexcept
{
    return glm::lookAt(glm::dvec3{ex, ey, ez},
                       glm::dvec3{cx, cy, cz},
                       glm::dvec3{ux, uy, uz});
}

} // namespace sdk::lua::bindings::math
