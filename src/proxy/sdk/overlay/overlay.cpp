
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

// Forward-declared; defined in wndproc.cpp
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

    auto& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;

    // Classic Dear ImGui dark style — clean, recognizable, professional
    ImGui::StyleColorsDark();

    // 2x font scale for crisp, readable text on modern displays
    io.FontGlobalScale = 2.0f;

    ImGui_ImplWin32_Init(g_ctx.window);
    ImGui_ImplOpenGL3_Init(k_glsl_version);

    g_ctx.imgui_initialized.store(true, std::memory_order::release);
    spdlog::info("[sdk] imgui initialized (classic dark, 2x scale)");
}

// ─── Built-in SDK status bar ─────────────────────────────────────────────────
// Rendered after Lua callbacks so it always sits on top.
// Non-interactive, compact, professional.

void render_status_bar()
{
    const auto& io = ImGui::GetIO();

    constexpr float k_bar_height = 22.0f;
    const ImVec2    bar_pos(0.0f, io.DisplaySize.y - k_bar_height);
    const ImVec2    bar_size(io.DisplaySize.x, k_bar_height);

    ImGui::SetNextWindowPos(bar_pos);
    ImGui::SetNextWindowSize(bar_size);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 2.0f));

    constexpr int k_flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar
        | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse
        | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBringToFrontOnFocus
        | ImGuiWindowFlags_NoInputs;

    ImGui::Begin("##sdk_status", nullptr, k_flags);

    // API indicator
    ImGui::TextUnformatted("[");
    ImGui::SameLine(0.0f, 0.0f);
    ImGui::TextColored(ImVec4(0.30f, 0.95f, 0.35f, 1.0f), "OpenGL");
    ImGui::SameLine(0.0f, 0.0f);
    ImGui::TextUnformatted("]");

    ImGui::SameLine();
    ImGui::TextUnformatted("|");
    ImGui::SameLine();

    // Version
    ImGui::TextUnformatted("v");
    ImGui::SameLine(0.0f, 0.0f);
    ImGui::TextColored(ImVec4(0.65f, 0.80f, 1.0f, 1.0f), "%s", sdk::k_version);

    ImGui::SameLine();
    ImGui::TextUnformatted("|");
    ImGui::SameLine();

    // Status
    ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.60f, 1.0f),
                       "overlay + cheats active");

    ImGui::End();
    ImGui::PopStyleVar(3);
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

    // Built-in status bar — always on top, non-interactive
    if (g_ctx.show_ui.load(std::memory_order::relaxed))
    {
        render_status_bar();
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void shutdown()
{
    if (g_ctx.imgui_initialized.load(std::memory_order::acquire))
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }
}

} // namespace sdk::overlay
