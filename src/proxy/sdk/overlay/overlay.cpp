
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

// ---------------------------------------------------------------------------
// Valve / Source-engine-inspired dark theme
// ---------------------------------------------------------------------------

void apply_valve_theme()
{
    auto& style = ImGui::GetStyle();

    // -- Rounded corners (subtle, professional) --
    style.WindowRounding    = 5.0f;
    style.FrameRounding     = 4.0f;
    style.GrabRounding      = 4.0f;
    style.ScrollbarRounding = 5.0f;
    style.TabRounding       = 5.0f;
    style.PopupRounding     = 5.0f;
    style.ChildRounding     = 5.0f;

    // -- Borders --
    style.WindowBorderSize = 0.0f;
    style.FrameBorderSize  = 1.0f;
    style.PopupBorderSize  = 1.0f;
    style.TabBorderSize    = 1.0f;

    // -- Spacing (roomier, modern) --
    style.WindowPadding     = ImVec2(12, 12);
    style.FramePadding      = ImVec2(10, 6);
    style.CellPadding       = ImVec2(6, 4);
    style.ItemSpacing       = ImVec2(10, 8);
    style.ItemInnerSpacing  = ImVec2(8, 5);
    style.IndentSpacing     = 24.0f;
    style.ScrollbarSize     = 14.0f;
    style.GrabMinSize       = 12.0f;

    // -- Title bar --
    style.WindowTitleAlign = ImVec2(0.5f, 0.5f);

    // -- Anti-aliasing --
    style.AntiAliasedLines = true;
    style.AntiAliasedFill  = true;

    // -- Colors: deep charcoal + blue accent (Valve Hammer / CS:S style) --
    ImVec4* c = style.Colors;

    // Backgrounds
    c[ImGuiCol_WindowBg]             = ImVec4(0.10f, 0.10f, 0.12f, 0.95f);
    c[ImGuiCol_ChildBg]              = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
    c[ImGuiCol_PopupBg]              = ImVec4(0.12f, 0.12f, 0.15f, 0.96f);

    // Borders
    c[ImGuiCol_Border]               = ImVec4(0.22f, 0.22f, 0.28f, 0.60f);

    // Title bar
    c[ImGuiCol_TitleBg]              = ImVec4(0.06f, 0.06f, 0.08f, 1.00f);
    c[ImGuiCol_TitleBgActive]        = ImVec4(0.10f, 0.10f, 0.13f, 1.00f);
    c[ImGuiCol_TitleBgCollapsed]     = ImVec4(0.06f, 0.06f, 0.08f, 1.00f);

    // Headers (tree nodes, collapsing)
    c[ImGuiCol_Header]               = ImVec4(0.16f, 0.16f, 0.20f, 0.55f);
    c[ImGuiCol_HeaderHovered]        = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
    c[ImGuiCol_HeaderActive]         = ImVec4(0.26f, 0.59f, 0.98f, 0.55f);

    // Buttons
    c[ImGuiCol_Button]               = ImVec4(0.16f, 0.16f, 0.20f, 1.00f);
    c[ImGuiCol_ButtonHovered]        = ImVec4(0.26f, 0.59f, 0.98f, 0.45f);
    c[ImGuiCol_ButtonActive]         = ImVec4(0.26f, 0.59f, 0.98f, 0.70f);

    // Frames (input fields, sliders, combos)
    c[ImGuiCol_FrameBg]              = ImVec4(0.14f, 0.14f, 0.17f, 1.00f);
    c[ImGuiCol_FrameBgHovered]       = ImVec4(0.18f, 0.18f, 0.22f, 1.00f);
    c[ImGuiCol_FrameBgActive]        = ImVec4(0.20f, 0.20f, 0.25f, 1.00f);

    // Tabs
    c[ImGuiCol_Tab]                  = ImVec4(0.10f, 0.10f, 0.13f, 1.00f);
    c[ImGuiCol_TabHovered]           = ImVec4(0.26f, 0.59f, 0.98f, 0.45f);
    c[ImGuiCol_TabActive]            = ImVec4(0.18f, 0.18f, 0.22f, 1.00f);
    c[ImGuiCol_TabUnfocused]         = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
    c[ImGuiCol_TabUnfocusedActive]   = ImVec4(0.14f, 0.14f, 0.17f, 1.00f);

    // Slider / drag grabbers
    c[ImGuiCol_SliderGrab]           = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
    c[ImGuiCol_SliderGrabActive]     = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);

    // Check mark
    c[ImGuiCol_CheckMark]            = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);

    // Separators
    c[ImGuiCol_Separator]            = ImVec4(0.22f, 0.22f, 0.28f, 0.55f);
    c[ImGuiCol_SeparatorHovered]     = ImVec4(0.26f, 0.59f, 0.98f, 0.78f);
    c[ImGuiCol_SeparatorActive]      = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);

    // Resize grips
    c[ImGuiCol_ResizeGrip]           = ImVec4(0.26f, 0.59f, 0.98f, 0.20f);
    c[ImGuiCol_ResizeGripHovered]    = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
    c[ImGuiCol_ResizeGripActive]     = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);

    // Text
    c[ImGuiCol_Text]                 = ImVec4(0.90f, 0.90f, 0.92f, 1.00f);
    c[ImGuiCol_TextDisabled]         = ImVec4(0.48f, 0.48f, 0.52f, 1.00f);
    c[ImGuiCol_TextSelectedBg]       = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);

    // Scrollbar
    c[ImGuiCol_ScrollbarBg]          = ImVec4(0.06f, 0.06f, 0.08f, 1.00f);
    c[ImGuiCol_ScrollbarGrab]        = ImVec4(0.22f, 0.22f, 0.26f, 1.00f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.32f, 0.32f, 0.38f, 1.00f);
    c[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.42f, 0.42f, 0.48f, 1.00f);

    // Plot
    c[ImGuiCol_PlotLines]            = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
    c[ImGuiCol_PlotLinesHovered]     = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
    c[ImGuiCol_PlotHistogram]        = ImVec4(0.26f, 0.59f, 0.98f, 0.60f);
    c[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);

    // Nav / Docking (future-proof)
    c[ImGuiCol_NavHighlight]         = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
    c[ImGuiCol_DragDropTarget]       = ImVec4(0.26f, 0.59f, 0.98f, 0.90f);
    c[ImGuiCol_ModalWindowDimBg]     = ImVec4(0.00f, 0.00f, 0.00f, 0.50f);
}

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

    // Valve-style professional dark theme
    ImGui::StyleColorsDark();
    apply_valve_theme();

    // 2x global font scale for crisp, readable UI on modern displays
    io.FontGlobalScale = 2.0f;

    ImGui_ImplWin32_Init(g_ctx.window);
    ImGui_ImplOpenGL3_Init(k_glsl_version);

    g_ctx.imgui_initialized.store(true, std::memory_order::release);
    spdlog::info("[sdk] imgui initialized (valve theme, 2x scale)");
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
    if (g_ctx.imgui_initialized.load(std::memory_order::acquire))
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }
}

} // namespace sdk::overlay
