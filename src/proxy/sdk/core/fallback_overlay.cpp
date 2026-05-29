#include "fallback_overlay.hpp"

#include "sdk/core/context.hpp"

#include <safetyhook.hpp>
#include <spdlog/spdlog.h>

namespace sdk::fallback_overlay
{

namespace
{

// ─── Window proc subclass ────────────────────────────────────────────────────

LRESULT CALLBACK hk_fallback_wnd_proc(HWND   hwnd,
                                      UINT   msg,
                                      WPARAM wp,
                                      LPARAM lp)
{
    // Redraw warning banner on paint and timer ticks.
    if ((msg == WM_PAINT) || (msg == WM_TIMER))
    {
        // Let original paint first, then draw over it.
        LRESULT result = CallWindowProcW(g_ctx.fallback_orig_wnd_proc,
                                         hwnd, msg, wp, lp);

        RECT rc;
        GetClientRect(hwnd, &rc);

        HDC hdc = GetDC(hwnd);
        if (hdc == nullptr)
        {
            return result;
        }

        // Dark semi-transparent banner at bottom.
        constexpr int k_banner_h = 26;
        RECT          banner{ 0,
                              rc.bottom - k_banner_h,
                              rc.right,
                              rc.bottom };

        HBRUSH bg = CreateSolidBrush(RGB(18, 18, 20));
        FillRect(hdc, &banner, bg);
        DeleteObject(bg);

        // Thin red accent line.
        HPEN   line_pen = CreatePen(PS_SOLID, 2, RGB(220, 60, 60));
        HPEN   old_pen  = static_cast<HPEN>(SelectObject(hdc, line_pen));
        MoveToEx(hdc, banner.left, banner.top, nullptr);
        LineTo(hdc, banner.right, banner.top);
        SelectObject(hdc, old_pen);
        DeleteObject(line_pen);

        // Warning text.
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(240, 100, 100));

        HFONT font = CreateFontW(
            15, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Consolas");

        HFONT old_font = static_cast<HFONT>(SelectObject(hdc, font));

        DrawTextW(
            hdc,
            L"[ LIMITED MODE — no OpenGL | cheats & input hooks only ]",
            -1,
            &banner,
            DT_SINGLELINE | DT_CENTER | DT_VCENTER);

        SelectObject(hdc, old_font);
        DeleteObject(font);
        ReleaseDC(hwnd, hdc);

        return result;
    }

    if (msg == WM_DESTROY)
    {
        KillTimer(hwnd, 1);
    }

    return CallWindowProcW(g_ctx.fallback_orig_wnd_proc, hwnd, msg, wp, lp);
}

// ─── CreateWindowExW hook ────────────────────────────────────────────────────

HWND WINAPI hk_create_window_ex_w(DWORD     ex_style,
                                   LPCWSTR   class_name,
                                   LPCWSTR   window_name,
                                   DWORD     style,
                                   int       x,
                                   int       y,
                                   int       w,
                                   int       h,
                                   HWND      parent,
                                   HMENU     menu,
                                   HINSTANCE instance,
                                   LPVOID    param)
{
    using fn_t = decltype(&CreateWindowExW);
    auto orig  = reinterpret_cast<fn_t>(
        g_ctx.fallback_create_window_hook.trampoline().address());

    HWND hwnd = orig(ex_style, class_name, window_name, style, x, y, w, h,
                     parent, menu, instance, param);

    // Capture the first sizeable, visible, non-tool game window.
    if ((hwnd != nullptr) && (g_ctx.fallback_window == nullptr))
    {
        if ((w > 200) && (h > 200) && (style & WS_VISIBLE)
            && !(ex_style & WS_EX_TOOLWINDOW))
        {
            g_ctx.fallback_window       = hwnd;
            g_ctx.fallback_orig_wnd_proc = reinterpret_cast<WNDPROC>(
                SetWindowLongPtrW(hwnd,
                                  GWLP_WNDPROC,
                                  reinterpret_cast<LONG_PTR>(
                                      hk_fallback_wnd_proc)));

            // Redraw banner every 500 ms so it stays visible even when the
            // game does not repaint (common with D3D exclusive mode).
            SetTimer(hwnd, 1, 500, nullptr);

            spdlog::info(
                "[fallback] captured window '{}' ({}x{}) — warning banner active",
                window_name ? window_name : L"<unnamed>",
                w,
                h);
        }
    }

    return hwnd;
}

} // namespace

// ─── Public API ──────────────────────────────────────────────────────────────

void install()
{
    spdlog::info("[fallback] installing CreateWindowExW hook");

    g_ctx.fallback_create_window_hook = safetyhook::create_inline(
        reinterpret_cast<void*>(CreateWindowExW),
        reinterpret_cast<void*>(hk_create_window_ex_w));
}

void uninstall()
{
    if ((g_ctx.fallback_window != nullptr)
        && (g_ctx.fallback_orig_wnd_proc != nullptr))
    {
        KillTimer(g_ctx.fallback_window, 1);
        SetWindowLongPtrW(
            g_ctx.fallback_window,
            GWLP_WNDPROC,
            reinterpret_cast<LONG_PTR>(g_ctx.fallback_orig_wnd_proc));
    }

    g_ctx.fallback_create_window_hook.reset();
    g_ctx.fallback_window          = nullptr;
    g_ctx.fallback_orig_wnd_proc   = nullptr;

    spdlog::info("[fallback] uninstalled");
}

} // namespace sdk::fallback_overlay
