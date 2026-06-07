// sdk/platform/window.cpp — WinAPI implementation of window abstraction
//
// All windows.h code isolated here. Public header remains pure C++23.

#include "sdk/platform/window.hpp"

#include <windows.h>

namespace sdk::platform
{

// ─── window_subclass::impl ─────────────────────────────────────────────────

struct window_subclass::impl
{
    HWND              hwnd{};
    WNDPROC           original_proc{};
    wnd_proc_fn       handler;

    impl(HWND h, wnd_proc_fn fn)
        : hwnd(h)
        , handler(std::move(fn))
    {
        // Store 'this' in window user data for WndProc access
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
        original_proc = reinterpret_cast<WNDPROC>(
            SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&static_wnd_proc)));
    }

    ~impl()
    {
        if (hwnd && original_proc)
        {
            SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(original_proc));
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        }
    }

    static LRESULT CALLBACK static_wnd_proc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
    {
        auto* self = reinterpret_cast<impl*>(GetWindowLongPtrW(h, GWLP_USERDATA));
        if (self && self->handler && self->handler(msg, wp, lp))
        {
            return 0; // Message handled
        }
        auto* orig = self ? self->original_proc : nullptr;
        return orig ? CallWindowProcW(orig, h, msg, wp, lp)
                    : DefWindowProcW(h, msg, wp, lp);
    }
};

// ─── window_subclass implementation ────────────────────────────────────────

window_subclass::window_subclass(window_handle* hwnd, wnd_proc_fn handler)
    : pimpl_(std::make_unique<impl>(reinterpret_cast<HWND>(hwnd), std::move(handler)))
{
}

window_subclass::~window_subclass() = default;

window_subclass::window_subclass(window_subclass&&) noexcept = default;
auto window_subclass::operator=(window_subclass&&) noexcept -> window_subclass& = default;

// ─── get_window_rect ───────────────────────────────────────────────────────

auto get_window_rect(window_handle* hwnd) noexcept -> rect
{
    RECT r{};
    if (hwnd && ::GetWindowRect(reinterpret_cast<HWND>(hwnd), &r))
    {
        return { r.left, r.top, r.right, r.bottom };
    }
    return {};
}

} // namespace sdk::platform
