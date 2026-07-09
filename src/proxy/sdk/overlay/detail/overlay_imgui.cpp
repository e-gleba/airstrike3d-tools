#include "sdk/overlay/overlay.hpp"

#include "sdk/core/context.hpp"
#include "sdk/core/detail/context_state.hpp"
#include "sdk/core/logging.hpp"
#include "sdk/overlay/detail/imgui_impl_d3d8_as3d.hpp"

#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_win32.h>
#include <windows.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND,
                                                             UINT,
                                                             WPARAM,
                                                             LPARAM);

namespace sdk::overlay
{

namespace
{

enum class renderer : std::uint8_t
{
    none,
    opengl,
    direct3d8
};

renderer g_renderer{ renderer::none };

LRESULT CALLBACK hk_wnd_proc(HWND h, UINT m, WPARAM w, LPARAM l);

[[nodiscard]] bool init_common(HWND window)
{
    if (g_ctx.imgui_initialized.load(std::memory_order::acquire))
    {
        return true;
    }

    sdk::detail::g_state.window = window;
    if (window == nullptr)
    {
        return false;
    }

    sdk::detail::g_state.original_wnd_proc =
        reinterpret_cast<WNDPROC>(SetWindowLongPtrA(
            window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(hk_wnd_proc)));
    if (sdk::detail::g_state.original_wnd_proc == nullptr)
    {
        return false;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    auto& io{ ImGui::GetIO() };
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;

    ImGui::StyleColorsDark();
    io.FontAllowUserScaling = true;

    if (!ImGui_ImplWin32_Init(window))
    {
        ImGui::DestroyContext();
        return false;
    }

    g_ctx.imgui_initialized.store(true, std::memory_order_release);
    return true;
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

    return CallWindowProc(sdk::detail::g_state.original_wnd_proc, h, m, w, l);
}

} // namespace

void init_opengl(std::uintptr_t native_device_context)
{
    if (g_renderer != renderer::none)
    {
        return;
    }

    const auto dc = reinterpret_cast<HDC>(native_device_context);

    if (wglGetCurrentContext() == nullptr || !init_common(WindowFromDC(dc)))
    {
        return;
    }

    if (!ImGui_ImplOpenGL3_Init(std::string{ k_glsl_version }.c_str()))
    {
        shutdown();
        return;
    }

    g_renderer = renderer::opengl;
    g_ctx.imgui_initialized.store(true, std::memory_order_release);
    sdk::log_info("overlay initialized (OpenGL)");
}

void init_direct3d8(std::uintptr_t native_device, std::uintptr_t native_window)
{
    if (g_renderer != renderer::none)
    {
        return;
    }

    if (!init_common(reinterpret_cast<HWND>(native_window)) ||
        !detail::d3d8_init(reinterpret_cast<IDirect3DDevice8*>(native_device)))
    {
        shutdown();
        return;
    }

    g_renderer = renderer::direct3d8;
    g_ctx.imgui_initialized.store(true, std::memory_order_release);
    sdk::log_info("overlay initialized (Direct3D 8)");
}

void render()
{
    if (g_renderer == renderer::opengl)
    {
        ImGui_ImplOpenGL3_NewFrame();
    }
    else if (g_renderer == renderer::direct3d8)
    {
        detail::d3d8_new_frame();
    }
    else
    {
        return;
    }

    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    g_ctx.cb.on_overlay.invoke();

    ImGui::Render();
    if (g_renderer == renderer::opengl)
    {
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }
    else
    {
        detail::d3d8_render_draw_data(ImGui::GetDrawData());
    }
}

void invalidate_device_objects() noexcept
{
    if (g_renderer == renderer::direct3d8)
    {
        detail::d3d8_invalidate_device_objects();
    }
}

void shutdown() noexcept
{
    if (g_ctx.imgui_initialized.load())
    {
        if (g_renderer == renderer::opengl)
        {
            ImGui_ImplOpenGL3_Shutdown();
        }
        else if (g_renderer == renderer::direct3d8)
        {
            detail::d3d8_shutdown();
        }
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        g_ctx.imgui_initialized.store(false);
    }

    if ((sdk::detail::g_state.window != nullptr) &&
        (sdk::detail::g_state.original_wnd_proc != nullptr))
    {
        SetWindowLongPtrA(
            sdk::detail::g_state.window,
            GWLP_WNDPROC,
            reinterpret_cast<LONG_PTR>(sdk::detail::g_state.original_wnd_proc));
    }
    sdk::detail::g_state.window            = nullptr;
    sdk::detail::g_state.original_wnd_proc = nullptr;
    g_renderer                             = renderer::none;
}

} // namespace sdk::overlay
