// sdk/math/glm_adapter.cpp — GLM ↔ sdk::math conversion (GLM isolated here)
//
// All GLM code lives in this .cpp file. Public headers remain pure C++23.

#include "sdk/math/types.hpp"
#include "sdk/math/mat4.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace sdk::math
{

// ─── GLM ↔ sdk::math conversion helpers (internal) ───────────────────────

namespace
{

[[nodiscard]] constexpr auto to_glm(vec3 const& v) noexcept -> glm::dvec3
{
    return { v.x, v.y, v.z };
}

[[nodiscard]] constexpr auto from_glm(glm::dvec3 const& v) noexcept -> vec3
{
    return { v.x, v.y, v.z };
}

[[nodiscard]] auto from_glm(glm::dmat4 const& m) noexcept -> mat4
{
    mat4 result{};
    auto ptr = glm::value_ptr(m);
    for (std::size_t i = 0; i < 16; ++i)
    {
        result.m[i] = ptr[i];
    }
    return result;
}

} // namespace

// ─── Matrix construction (GLM backend) ────────────────────────────────────

auto look_at(vec3 const& eye, vec3 const& center, vec3 const& up) noexcept -> mat4
{
    auto glm_mat = glm::lookAt(to_glm(eye), to_glm(center), to_glm(up));
    return from_glm(glm_mat);
}

auto perspective(double fov_radians, double aspect, double near, double far) noexcept -> mat4
{
    auto glm_mat = glm::perspective(fov_radians, aspect, near, far);
    return from_glm(glm_mat);
}

auto ortho(double left, double right, double bottom, double top, double near, double far) noexcept -> mat4
{
    auto glm_mat = glm::ortho(left, right, bottom, top, near, far);
    return from_glm(glm_mat);
}

// ─── Matrix operations (GLM backend) ──────────────────────────────────────

auto multiply(mat4 const& a, mat4 const& b) noexcept -> mat4
{
    glm::dmat4 glm_a, glm_b;
    for (std::size_t i = 0; i < 16; ++i)
    {
        glm_a[i / 4][i % 4] = a.m[i];
        glm_b[i / 4][i % 4] = b.m[i];
    }
    auto result = glm_a * glm_b;
    return from_glm(result);
}

auto transpose(mat4 const& m) noexcept -> mat4
{
    glm::dmat4 glm_m;
    for (std::size_t i = 0; i < 16; ++i)
    {
        glm_m[i / 4][i % 4] = m.m[i];
    }
    return from_glm(glm::transpose(glm_m));
}

auto inverse(mat4 const& m) noexcept -> mat4
{
    glm::dmat4 glm_m;
    for (std::size_t i = 0; i < 16; ++i)
    {
        glm_m[i / 4][i % 4] = m.m[i];
    }
    return from_glm(glm::inverse(glm_m));
}

auto transform_point(mat4 const& m, vec3 const& p) noexcept -> vec3
{
    glm::dmat4 glm_m;
    for (std::size_t i = 0; i < 16; ++i)
    {
        glm_m[i / 4][i % 4] = m.m[i];
    }
    auto glm_p = glm::dvec4(p.x, p.y, p.z, 1.0);
    auto result = glm_m * glm_p;
    return { result.x / result.w, result.y / result.w, result.z / result.w };
}

auto transform_direction(mat4 const& m, vec3 const& d) noexcept -> vec3
{
    glm::dmat4 glm_m;
    for (std::size_t i = 0; i < 16; ++i)
    {
        glm_m[i / 4][i % 4] = m.m[i];
    }
    auto glm_d = glm::dvec4(d.x, d.y, d.z, 0.0);
    auto result = glm_m * glm_d;
    return { result.x, result.y, result.z };
}

} // namespace sdk::math
