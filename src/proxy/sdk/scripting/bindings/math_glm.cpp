// src/proxy/sdk/scripting/bindings/math_glm.cpp
// Mathematical functions using GLM backend.
// Pure C++ — no scripting backend types.

#include "sdk/scripting/bindings/math.hpp"

#include <algorithm>
#include <cmath>
#include <sol/sol.hpp>

namespace sdk::scripting::bindings::math
{

constexpr double k_pi = 3.14159265358979323846;

double radians(double degrees) noexcept
{
    return degrees * k_pi / 180.0;
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
    return std::fmod(v, d);
}

double clamp(double v, double lo, double hi) noexcept
{
    return std::clamp(v, lo, hi);
}

vec3 normalize(double x, double y, double z) noexcept
{
    const double len = std::sqrt(x * x + y * y + z * z);
    if (len < 1e-10) return {0.0, 0.0, 0.0};
    return {x / len, y / len, z / len};
}

vec3 cross(double ax, double ay, double az, double bx, double by, double bz) noexcept
{
    return {
        ay * bz - az * by,
        az * bx - ax * bz,
        ax * by - ay * bx
    };
}

mat4 lookat_matrix(double ex, double ey, double ez,
                   double cx, double cy, double cz,
                   double ux, double uy, double uz) noexcept
{
    // Forward vector (eye to center)
    const double fx = cx - ex;
    const double fy = cy - ey;
    const double fz = cz - ez;
    const double flen = std::sqrt(fx * fx + fy * fy + fz * fz);
    
    if (flen < 1e-10) {
        // Identity matrix if eye == center
        return {
            1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 1, 0,
            0, 0, 0, 1
        };
    }
    
    const double fnx = fx / flen;
    const double fny = fy / flen;
    const double fnz = fz / flen;
    
    // Right vector (forward × up)
    double rx = fny * uz - fnz * uy;
    double ry = fnz * ux - fnx * uz;
    double rz = fnx * uy - fny * ux;
    const double rlen = std::sqrt(rx * rx + ry * ry + rz * rz);
    
    if (rlen < 1e-10) {
        // Up and forward are parallel, use default up
        return {
            1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 1, 0,
            -ex, -ey, -ez, 1
        };
    }
    
    rx /= rlen;
    ry /= rlen;
    rz /= rlen;
    
    // Recompute up (right × forward)
    const double upx = ry * fnz - rz * fny;
    const double upy = rz * fnx - rx * fnz;
    const double upz = rx * fny - ry * fnx;
    
    // Column-major 4x4 matrix
    return {
        rx, upx, -fnx, 0,
        ry, upy, -fny, 0,
        rz, upz, -fnz, 0,
        -(rx * ex + ry * ey + rz * ez),
        -(upx * ex + upy * ey + upz * ez),
        fnx * ex + fny * ey + fnz * ez,
        1
    };
}

// ── Registration function for Lua bindings ──────────────────────────────────

void register_math(sol::state& lua)
{
    auto math = lua["math"].get_or_create<sol::table>();
    
    math["radians"] = &radians;
    math["cos"] = &cos;
    math["sin"] = &sin;
    math["mod"] = &mod;
    math["clamp"] = &clamp;
    
    math["normalize"] = [](double x, double y, double z) {
        auto v = normalize(x, y, z);
        return std::make_tuple(v.x, v.y, v.z);
    };
    
    math["cross"] = [](double ax, double ay, double az,
                       double bx, double by, double bz) {
        auto v = cross(ax, ay, az, bx, by, bz);
        return std::make_tuple(v.x, v.y, v.z);
    };
    
    math["lookat_matrix"] = [](double ex, double ey, double ez,
                               double cx, double cy, double cz,
                               double ux, double uy, double uz) {
        return lookat_matrix(ex, ey, ez, cx, cy, cz, ux, uy, uz);
    };
}

} // namespace sdk::scripting::bindings::math
