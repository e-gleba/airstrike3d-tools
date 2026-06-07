#pragma once

// sdk/platform/types.hpp — Opaque platform handle types (pure C++23)
//
// No windows.h, no platform-specific types exposed.

#include <cstdint>

namespace sdk::platform
{

// Opaque handle types (forward declarations, PIMPL-ready)
struct window_handle;
struct module_handle;

// Platform-independent rectangle
struct rect
{
    std::int32_t left{}, top{}, right{}, bottom{};

    [[nodiscard]] constexpr auto width() const noexcept -> std::int32_t
    {
        return right - left;
    }

    [[nodiscard]] constexpr auto height() const noexcept -> std::int32_t
    {
        return bottom - top;
    }
};

// Cursor position
struct cursor_pos
{
    std::int32_t x{}, y{};
};

} // namespace sdk::platform
