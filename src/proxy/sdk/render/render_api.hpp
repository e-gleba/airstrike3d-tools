#pragma once

// sdk/render/render_api.hpp — Render API enumeration (pure C++23)

#include <cstdint>

namespace sdk::render
{

enum class render_api : std::uint8_t
{
    unknown,
    opengl,
    directx
};

} // namespace sdk::render
