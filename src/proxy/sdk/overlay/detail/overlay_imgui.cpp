#include "sdk/overlay/overlay.hpp"

#include "sdk/core/context.hpp"
#include "sdk/core/detail/context_state.hpp"
#include "sdk/core/logging.hpp"

#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_win32.h>
#include <mutex>
#include <windows.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND,
                                                             UINT,
                                                             WPARAM,
                                                             LPARAM);

namespace sdk::overlay
{

namespace
{

LRESULT CALLBACK hk_wnd_proc(HWND h, UINT m, WPARAM w, LPARAM l);

void init_imgui(HDC dc)
{
    detail::g_state.window = WindowFromDC(dc);
    if (detail::g_state.window == nullptr)
    {
        return;
    }

    detail::g_state.original_wnd_proc = reinterpret_cast<WNDPROC>(SetWindowLongPtrA(
        detail::g_state.window,
        GWLP_WNDPROC,
        reinterpret_cast<LONG_PTR>(hk_wnd_proc)));

    ImGui::CreateContext();

    auto& io{ ImGui::GetIO() };
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;

    ImGui::StyleColorsDark();
    io.FontAllowUserScaling = true;

    ImGui_ImplWin32_Init(detail::g_state.window);
    ImGui_ImplOpenGL3_Init(std::string{ k_glsl_version }.c_str());

    g_ctx.imgui_initialized.store(true);
    sdk::log_info("overlay initialized (ImGui backend)");
}

LRESULT CALLBACK hk_wnd_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (m == WM_KEYDOWN) [[unlikely]]
    {
        if (w == static_cast<WPARAM>(k_ui_toggle_key)) [[unlikely]]
        {
            g_ctx.show_ui = !g_ctx.show_ui.load();
            return 0;
        }
        if (g_ctx.cb.on_key_down.invoke(static_cast<std::int32_t>(w)))
        {
            return 0;
        }
    }

    if (!g_ctx.should_unload.load() && g_ctx.show_ui.load() &&
        ImGui_ImplWin32_WndProcHandler(h, m, w, l))
    {
        return 1;
    }

    return CallWindowProc(detail::g_state.original_wnd_proc, h, m, w, l);
}

} // namespace

void init(std::uintptr_t native_device_context)
{
    static std::once_flag flag;
    const auto dc = reinterpret_cast<HDC>(native_device_context);

    if (wglGetCurrentContext() == nullptr)
    {
        return;
    }

    std::call_once(flag, init_imgui, dc);
}

void render()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    g_ctx.cb.on_overlay.invoke();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void shutdown()
{
    if (g_ctx.imgui_initialized.load())
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        g_ctx.imgui_initialized.store(false);
    }
}

} // namespace sdk::overlay
