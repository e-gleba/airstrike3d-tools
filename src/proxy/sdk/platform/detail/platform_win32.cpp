#include "sdk/platform/platform.hpp"

#include "sdk/core/context.hpp"
#include "sdk/core/detail/context_state.hpp"
#include "sdk/core/logging.hpp"

#include <format>
#include <ranges>
#include <tuple>
#include <windows.h>

namespace sdk::platform
{

namespace detail
{

[[nodiscard]] bool is_key_down_impl(int vk) noexcept
{
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}

[[nodiscard]] std::tuple<int, int> cursor_pos_impl() noexcept
{
    POINT p{};
    GetCursorPos(&p);
    return { p.x, p.y };
}

[[nodiscard]] std::tuple<int, int, int, int> window_rect_impl(HWND hwnd) noexcept
{
    RECT r{};
    if (hwnd != nullptr)
    {
        GetWindowRect(hwnd, &r);
    }
    return { r.left, r.top, r.right, r.bottom };
}

} // namespace detail

bool is_key_down(std::int32_t vk) noexcept
{
    return detail::is_key_down_impl(vk);
}

vec2 get_cursor_pos() noexcept
{
    const auto [x, y] = detail::cursor_pos_impl();
    return { static_cast<double>(x), static_cast<double>(y) };
}

void set_cursor_pos(std::int32_t x, std::int32_t y) noexcept
{
    SetCursorPos(x, y);
}

void show_cursor(bool visible) noexcept
{
    ShowCursor(visible ? TRUE : FALSE);
}

rect get_window_rect() noexcept
{
    const auto [left, top, right, bottom] =
        detail::window_rect_impl(sdk::detail::g_state.window);
    return { left, top, right, bottom };
}

void log_info(std::string_view message)
{
    sdk::log_info(std::format("[scripting] {}", message));
}

void log_warn(std::string_view message)
{
    sdk::log_warn(std::format("[scripting] {}", message));
}

void log_error(std::string_view message)
{
    sdk::log_error(std::format("[scripting] {}", message));
}

void send_chars(std::string_view chars) noexcept
{
    std::ranges::for_each(chars, [](char c) {
        INPUT input{};
        input.type       = INPUT_KEYBOARD;
        input.ki.wVk     = 0;
        input.ki.wScan   = static_cast<WORD>(c);
        input.ki.dwFlags = KEYEVENTF_UNICODE;
        SendInput(1, &input, sizeof(INPUT));
        input.ki.dwFlags |= KEYEVENTF_KEYUP;
        SendInput(1, &input, sizeof(INPUT));
    });
}

} // namespace sdk::platform
