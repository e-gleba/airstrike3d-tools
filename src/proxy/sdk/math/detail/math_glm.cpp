#include "sdk/math/math.hpp"

#include "sdk/core/contract.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace sdk::math
{

namespace
{

constexpr double k_epsilon = std::numeric_limits<double>::epsilon();

} // namespace

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

double mod(double v, double divisor)
{
    require(divisor != 0.0, "math::mod: divisor must not be zero");
    return glm::mod(v, divisor);
}

double clamp(double v, double lo, double hi)
{
    require(lo <= hi, "math::clamp: lower bound must not exceed upper bound");
    return glm::clamp(v, lo, hi);
}

vec3 normalize(double x, double y, double z)
{
    const double len_sq = (x * x) + (y * y) + (z * z);
    require(len_sq > k_epsilon, "math::normalize: zero-length vector");
    const auto v = glm::normalize(glm::dvec3{ x, y, z });
    return { v.x, v.y, v.z };
}

vec3 cross(double ax, double ay, double az, double bx, double by, double bz) noexcept
{
    const auto v = glm::cross(glm::dvec3{ ax, ay, az }, glm::dvec3{ bx, by, bz });
    return { v.x, v.y, v.z };
}

mat4 lookat_matrix(double ex, double ey, double ez,
                   double cx, double cy, double cz,
                   double ux, double uy, double uz) noexcept
{
    const auto m = glm::lookAt(glm::dvec3{ ex, ey, ez },
                               glm::dvec3{ cx, cy, cz },
                               glm::dvec3{ ux, uy, uz });
    mat4 result{};
    const double* ptr = glm::value_ptr(m);
    std::copy(ptr, ptr + 16, result.begin());
    return result;
}

} // namespace sdk::math
