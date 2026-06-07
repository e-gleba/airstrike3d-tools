#include "overlay.hpp"
#include "sdk/core/context.hpp"

#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_win32.h>
#include <mutex>
#include <spdlog/spdlog.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND,
                                                             UINT,
                                                             WPARAM,
                                                             LPARAM);

namespace sdk::overlay
{

LRESULT CALLBACK hk_wnd_proc(HWND, UINT, WPARAM, LPARAM);

namespace
{

void init_imgui(HDC dc)
{
    g_ctx.window = WindowFromDC(dc);
    if (g_ctx.window == nullptr)
    {
        return;
    }

    g_ctx.original_wnd_proc = reinterpret_cast<WNDPROC>(SetWindowLongPtrA(
        g_ctx.window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(hk_wnd_proc)));

    ImGui::CreateContext();

    auto& io{ ImGui::GetIO() };
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;

    ImGui::StyleColorsDark();

    io.FontAllowUserScaling = true;

    ImGui_ImplWin32_Init(g_ctx.window);
    ImGui_ImplOpenGL3_Init(k_glsl_version);

    g_ctx.imgui_initialized = true;
    spdlog::info("[sdk] imgui initialized (classic dark, 2x scale)");
}

} // namespace

void init(HDC dc)
{
    static std::once_flag flag;

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
    if (g_ctx.imgui_initialized)
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }
}

} // namespace sdk::overlay