/// @file types.hpp
/// @brief Standard-library value types for the public SDK interface.

#pragma once

#include <array>
#include <cstdint>

namespace sdk
{

enum class render_api : std::uint8_t
{
    unknown,
    opengl,
    directx
};

struct vec2 final
{
    double x{};
    double y{};
};

struct vec3 final
{
    double x{};
    double y{};
    double z{};
};

struct rect final
{
    int left{};
    int top{};
    int right{};
    int bottom{};
};

using matrix_mode = std::int32_t;
using mat4        = std::array<double, 16>;

} // namespace sdk
