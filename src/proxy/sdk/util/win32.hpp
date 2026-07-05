#pragma once
#include <array>
#include <tuple>
#include <windows.h>

namespace sdk::win32
{

[[nodiscard]] inline void* proc_addr(const wchar_t* mod,
                                     const char*    fn) noexcept
{
    return reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(mod), fn));
}

[[nodiscard]] inline bool is_key_down(int vk) noexcept
{
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}

[[nodiscard]] inline std::tuple<int, int> cursor_pos() noexcept
{
    POINT p{};
    GetCursorPos(&p);
    return { p.x, p.y };
}

[[nodiscard]] inline std::tuple<int, int, int, int> window_rect(
    HWND hwnd) noexcept
{
    RECT r{};
    if (hwnd != nullptr)
    {
        GetWindowRect(hwnd, &r);
    }
    return { r.left, r.top, r.right, r.bottom };
}

} // namespace sdk::win32
