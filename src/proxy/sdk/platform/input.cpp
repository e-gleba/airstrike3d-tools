// sdk/platform/input.cpp — WinAPI implementation of input queries

#include "sdk/platform/input.hpp"

#include <windows.h>

namespace sdk::platform
{

auto is_key_down(int vk_code) noexcept -> bool
{
    return (GetAsyncKeyState(vk_code) & 0x8000) != 0;
}

auto get_cursor_pos() noexcept -> cursor_pos
{
    POINT pt{};
    GetCursorPos(&pt);
    return { pt.x, pt.y };
}

void set_cursor_pos(std::int32_t x, std::int32_t y) noexcept
{
    SetCursorPos(x, y);
}

void show_cursor(bool visible) noexcept
{
    ShowCursor(visible ? TRUE : FALSE);
}

} // namespace sdk::platform
