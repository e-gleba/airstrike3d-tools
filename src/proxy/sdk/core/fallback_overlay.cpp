#include "fallback_overlay.hpp"

#include "sdk/core/context.hpp"

#include <mutex>
#include <safetyhook.hpp>
#include <spdlog/spdlog.h>

namespace sdk::fallback_overlay
{

namespace
{

// ─── One-shot notification flag ──────────────────────────────────────────────

static std::once_flag g_notify_flag;

void show_notification_once()
{
    std::call_once(g_notify_flag, [] {
        const wchar_t* title   = L"AirStrike3D Proxy SDK";
        const wchar_t* message = nullptr;
        UINT           icon    = MB_ICONINFORMATION;

        switch (g_ctx.detected_api.load(std::memory_order::relaxed))
        {
        case render_api::directx:
            message = L"DirectX renderer detected.\n\n"
                      L"ImGui overlay is not available — only cheats and "
                      L"input hooks are active.\n"
                      L"Lua plugins are loaded and functional.\n\n"
                      L"A status banner will be shown at the bottom of the "
                      L"game window.";
            break;
        default:
            message = L"No supported render API detected.\n\n"
                      L"ImGui overlay is not available — only cheats and "
                      L"input hooks are active.\n"
                      L"Lua plugins are loaded and functional.\n\n"
                      L"A status banner will be shown at the bottom of the "
                      L"game window.";
            icon = MB_ICONWARNING;
            break;
        }

        spdlog::info("[fallback] showing notification dialog");

        MessageBoxW(nullptr, message, title,
                    MB_OK | icon | MB_TOPMOST | MB_SETFOREGROUND);

        spdlog::info("[fallback] notification dismissed by user");
    });
}

// ─── Helpers ─────────────────────────────────────────────────────────────────

const wchar_t* banner_text()
{
    switch (g_ctx.detected_api.load(std::memory_order::relaxed))
    {
    case render_api::directx:
        return L"[ DirectX  —  overlay not available  —  cheats & input "
               L"hooks only ]";
    default:
        return L"[ LIMITED MODE  —  no OpenGL  —  cheats & input hooks "
               L"only ]";
    }
}

// ─── Window proc subclass ────────────────────────────────────────────────────

LRESULT CALLBACK hk_fallback_wnd_proc(HWND   hwnd,
                                      UINT   msg,
                                      WPARAM wp,
                                      LPARAM lp)
{
    if ((msg == WM_PAINT) || (msg == WM_TIMER))
    {
        LRESULT result = CallWindowProcW(g_ctx.fallback_orig_wnd_proc,
                                         hwnd, msg, wp, lp);

        RECT rc;
        GetClientRect(hwnd, &rc);

        HDC hdc = GetDC(hwnd);
        if (hdc == nullptr)
        {
            return result;
        }

        // ── Banner background ────────────────────────────────────────────

        constexpr int k_banner_h = 28;
        RECT          banner{ 0,
                              rc.bottom - k_banner_h,
                              rc.right,
                              rc.bottom };

        HBRUSH bg = CreateSolidBrush(RGB(16, 16, 18));
        FillRect(hdc, &banner, bg);
        DeleteObject(bg);

        // ── Accent line — red for unknown, amber for DX ──────────────────

        COLORREF line_color =
            (g_ctx.detected_api.load(std::memory_order::relaxed)
                     == render_api::directx)
                ? RGB(220, 160, 40)
                : RGB(220, 55, 55);

        HPEN line_pen = CreatePen(PS_SOLID, 2, line_color);
        HPEN old_pen  = static_cast<HPEN>(SelectObject(hdc, line_pen));
        MoveToEx(hdc, banner.left, banner.top, nullptr);
        LineTo(hdc, banner.right, banner.top);
        SelectObject(hdc, old_pen);
        DeleteObject(line_pen);

        // ── Warning text ─────────────────────────────────────────────────

        SetBkMode(hdc, TRANSPARENT);

        HFONT font = CreateFontW(
            15, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Consolas");

        HFONT old_font = static_cast<HFONT>(SelectObject(hdc, font));

        // Left: API label
        RECT left = banner;
        left.right = left.left + 100;
        SetTextColor(hdc, RGB(100, 100, 110));
        DrawTextW(
            hdc,
            (g_ctx.detected_api.load(std::memory_order::relaxed)
                     == render_api::directx)
                ? L"[DirectX]"
                : L"[???]",
            -1, &left, DT_SINGLELINE | DT_LEFT | DT_VCENTER);

        // Centre: warning
        COLORREF text_color =
            (g_ctx.detected_api.load(std::memory_order::relaxed)
                     == render_api::directx)
                ? RGB(235, 180, 80)
                : RGB(235, 95, 95);

        SetTextColor(hdc, text_color);
        DrawTextW(hdc, banner_text(), -1, &banner,
                  DT_SINGLELINE | DT_CENTER | DT_VCENTER);

        SelectObject(hdc, old_font);
        DeleteObject(font);
        ReleaseDC(hwnd, hdc);

        // ── One-time notification ────────────────────────────────────────

        show_notification_once();

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

    if ((hwnd != nullptr) && (g_ctx.fallback_window == nullptr))
    {
        if ((w > 200) && (h > 200) && (style & WS_VISIBLE)
            && !(ex_style & WS_EX_TOOLWINDOW))
        {
            g_ctx.fallback_window        = hwnd;
            g_ctx.fallback_orig_wnd_proc = reinterpret_cast<WNDPROC>(
                SetWindowLongPtrW(hwnd,
                                  GWLP_WNDPROC,
                                  reinterpret_cast<LONG_PTR>(
                                      hk_fallback_wnd_proc)));

            SetTimer(hwnd, 1, 500, nullptr);

            spdlog::info(
                "[fallback] captured game window ({}x{}) — banner active",
                w, h);
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
    g_ctx.fallback_window        = nullptr;
    g_ctx.fallback_orig_wnd_proc = nullptr;

    spdlog::info("[fallback] uninstalled");
}

} // namespace sdk::fallback_overlay
