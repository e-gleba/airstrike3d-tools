// sdk/overlay/overlay.cpp — ImGui implementation of overlay
//
// ALL ImGui code isolated here. Public header remains pure C++23.

#include "sdk/overlay/overlay.hpp"

#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_opengl3.h>

#include <spdlog/spdlog.h>

namespace sdk::overlay
{

// ─── manager::impl ───────────────────────────────────────────────────────

struct manager::impl
{
    bool initialized = false;
    bool visible     = true;
    HWND hwnd        = nullptr;
    WNDPROC original_wndproc = nullptr;

    void init_imgui(HWND h, std::string_view glsl_version)
    {
        if (initialized)
        {
            return;
        }

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        ImGui::StyleColorsDark();

        ImGui_ImplWin32_Init(h);
        ImGui_ImplOpenGL3_Init(glsl_version.data());

        hwnd = h;
        initialized = true;
        spdlog::info("[overlay] ImGui initialized");
    }

    void shutdown_imgui()
    {
        if (!initialized)
        {
            return;
        }

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();

        initialized = false;
        spdlog::info("[overlay] ImGui shutdown");
    }

    void render_frame()
    {
        if (!initialized || !visible)
        {
            return;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // User code would draw here via callbacks

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }
};

// ─── manager implementation ──────────────────────────────────────────────

manager::manager() : pimpl_(std::make_unique<impl>()) {}

manager::~manager()
{
    if (pimpl_ && pimpl_->initialized)
    {
        shutdown();
    }
}

manager::manager(manager&&) noexcept = default;
auto manager::operator=(manager&&) noexcept -> manager& = default;

void manager::init(render::device_context* ctx, config cfg)
{
    if (!ctx)
    {
        spdlog::error("[overlay] null device context");
        return;
    }

    auto hwnd = reinterpret_cast<HWND>(ctx);
    pimpl_->init_imgui(hwnd, cfg.glsl_version);
    pimpl_->visible = cfg.auto_show;
}

void manager::render()
{
    pimpl_->render_frame();
}

void manager::shutdown()
{
    pimpl_->shutdown_imgui();
}

void manager::toggle_visibility() noexcept
{
    pimpl_->visible = !pimpl_->visible;
}

auto manager::is_visible() const noexcept -> bool
{
    return pimpl_->visible;
}

auto manager::process_message(std::uint32_t msg, std::uintptr_t wparam, std::intptr_t lparam) -> bool
{
    if (!pimpl_->initialized)
    {
        return false;
    }

    if (ImGui_ImplWin32_WndProcHandler(pimpl_->hwnd, msg, wparam, lparam))
    {
        return true;
    }

    return false;
}

// ─── Window procedure handler ───────────────────────────────────────────

auto wnd_proc_handler(platform::window_handle* hwnd,
                      std::uint32_t msg,
                      std::uintptr_t wparam,
                      std::intptr_t lparam,
                      manager* overlay) -> bool
{
    if (!overlay)
    {
        return false;
    }

    return overlay->process_message(msg, wparam, lparam);
}

} // namespace sdk::overlay
