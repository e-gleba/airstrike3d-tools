// src/proxy/sdk/scripting/bindings/math_glm.cpp
// Mathematical functions using GLM backend.
// Pure C++ — no scripting backend types.

#include "sdk/scripting/bindings/math.hpp"

#include <cmath>
#include <numbers>

namespace sdk::scripting::bindings::math
{

double radians(double degrees) noexcept
{
    return degrees * std::numbers::pi / 180.0;
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
    const auto len = std::sqrt(x * x + y * y + z * z);
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
    const auto fx = cx - ex;
    const auto fy = cy - ey;
    const auto fz = cz - ez;
    const auto flen = std::sqrt(fx * fx + fy * fy + fz * fz);
    
    if (flen < 1e-10) {
        // Identity matrix if eye == center
        return {
            1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 1, 0,
            0, 0, 0, 1
        };
    }
    
    const auto fnx = fx / flen;
    const auto fny = fy / flen;
    const auto fnz = fz / flen;
    
    // Right vector (forward × up)
    auto rx = fny * uz - fnz * uy;
    auto ry = fnz * ux - fnx * uz;
    auto rz = fnx * uy - fny * ux;
    const auto rlen = std::sqrt(rx * rx + ry * ry + rz * rz);
    
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
    const auto upx = ry * fnz - rz * fny;
    const auto upy = rz * fnx - rx * fnz;
    const auto upz = rx * fny - ry * fnx;
    
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

} // namespace sdk::scripting::bindings::math
