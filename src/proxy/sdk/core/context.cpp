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
    if (m == WM_KEYDOWN && w == k_ui_toggle_key)
    {
        g_ctx.show_ui.store(!g_ctx.show_ui.load());
        return 0;
    }

    if (m == WM_KEYDOWN)
    {
        if (g_ctx.cb.on_key_down.invoke_consuming(static_cast<int>(w)))
        {
            return 0;
        }
    }

    if (!g_ctx.should_unload.load() && g_ctx.show_ui.load() &&
        ImGui_ImplWin32_WndProcHandler(h, m, w, l) != 0)
    {
        return 1;
    }

    return CallWindowProc(g_ctx.original_wnd_proc, h, m, w, l);
}

} // namespace sdk::overlay