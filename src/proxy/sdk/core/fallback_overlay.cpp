#include "fallback_overlay.hpp"

#include "sdk/core/context.hpp"

#include <mutex>
#include <safetyhook.hpp>
#include <spdlog/spdlog.h>

namespace sdk::fallback_overlay
{

namespace
{

// ─── Custom message for deferred notification ────────────────────────────────

constexpr UINT k_wm_show_notify = WM_APP + 0x100;

// ─── One-shot notification flag ──────────────────────────────────────────────

static std::once_flag g_notify_flag;

void show_notification_once(HWND owner)
{
    std::call_once(g_notify_flag, [owner] {
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

        MessageBoxW(owner, message, title,
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

// ─── Window subclass helper ──────────────────────────────────────────────────

void subclass_window(HWND hwnd, int w, int h)
{
    g_ctx.fallback_window        = hwnd;
    g_ctx.fallback_orig_wnd_proc = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(hwnd,
                          GWLP_WNDPROC,
                          reinterpret_cast<LONG_PTR>(hk_fallback_wnd_proc)));

    SetTimer(hwnd, 1, 500, nullptr);

    // Defer notification — PostMessage so it runs outside the paint cycle
    PostMessageW(hwnd, k_wm_show_notify, 0, 0);

    spdlog::info(
        "[fallback] captured game window ({}x{}) — banner active",
        w, h);
}

// ─── EnumWindows callback — catch already-existing windows ───────────────────

BOOL CALLBACK enum_existing_windows(HWND hwnd, LPARAM /*lparam*/)
{
    if (g_ctx.fallback_window != nullptr)
    {
        return FALSE; // already captured
    }

    if (!IsWindowVisible(hwnd))
    {
        return TRUE;
    }

    LONG_PTR ex_style = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    if ((ex_style & WS_EX_TOOLWINDOW) != 0)
    {
        return TRUE;
    }

    RECT rc;
    if (!GetWindowRect(hwnd, &rc))
    {
        return TRUE;
    }

    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;

    if ((w <= 200) || (h <= 200))
    {
        return TRUE;
    }

    // Check it's a game process window (has a title, not a system window)
    wchar_t title[256]{};
    if (GetWindowTextW(hwnd, title,
                       static_cast<int>(std::size(title))) == 0)
    {
        return TRUE;
    }

    subclass_window(hwnd, w, h);
    return FALSE;
}

// ─── Window proc subclass ────────────────────────────────────────────────────

LRESULT CALLBACK hk_fallback_wnd_proc(HWND   hwnd,
                                      UINT   msg,
                                      WPARAM wp,
                                      LPARAM lp)
{
    // ── Deferred notification — safe to call MessageBox here ─────────────

    if (msg == k_wm_show_notify)
    {
        show_notification_once(hwnd);
        return 0;
    }

    // ── Paint/timer: draw GDI banner ─────────────────────────────────────

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

        // ── Accent line ──────────────────────────────────────────────────

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
            subclass_window(hwnd, w, h);
        }
    }

    return hwnd;
}

} // namespace

// ─── Public API ──────────────────────────────────────────────────────────────

void install()
{
    spdlog::info("[fallback] installing CreateWindowExW hook");

    // Hook future window creation
    g_ctx.fallback_create_window_hook = safetyhook::create_inline(
        reinterpret_cast<void*>(CreateWindowExW),
        reinterpret_cast<void*>(hk_create_window_ex_w));

    // Scan for already-existing game windows (race: game window created
    // before DX DLLs were loaded and on_dx_detected() fired)
    spdlog::info("[fallback] scanning existing windows...");
    EnumWindows(enum_existing_windows, 0);

    if (g_ctx.fallback_window != nullptr)
    {
        spdlog::info("[fallback] captured existing game window");
    }
    else
    {
        spdlog::info("[fallback] no existing window found — waiting for "
                       "CreateWindowExW");
    }
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
