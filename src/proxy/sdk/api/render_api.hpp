#pragma once

// sdk/api/render_api.hpp — Render API enumeration (pure C++23)

#include <cstdint>

namespace sdk
{

enum class render_api : std::uint8_t
{
    unknown,
    opengl,
    directx
};

} // namespace sdk
