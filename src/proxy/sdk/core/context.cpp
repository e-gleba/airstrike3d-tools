#include "sdk/core/context.hpp"

#include <imgui.h>
#include <imgui_impl_win32.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND,
                                                             UINT,
                                                             WPARAM,
                                                             LPARAM);

namespace sdk::overlay
{

[[nodiscard]] LRESULT CALLBACK hk_wnd_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (m == WM_KEYDOWN) [[unlikely]]
    {
        if (w == k_ui_toggle_key) [[unlikely]]
        {
            g_ctx.show_ui = !g_ctx.show_ui;
            return 0;
        }
        if (g_ctx.lua_engine && g_ctx.lua_engine->invoke_on_key_down(static_cast<int>(w)))
        {
            return 0;
        }
    }

    if (!g_ctx.should_unload && g_ctx.show_ui &&
        ImGui_ImplWin32_WndProcHandler(h, m, w, l))
    {
        return 1;
    }

    return CallWindowProc(g_ctx.original_wnd_proc, h, m, w, l);
}

} // namespace sdk::overlay
