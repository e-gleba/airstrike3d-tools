// sdk/core/engine.cpp — Engine implementation
//
// Composition root: wires together all abstraction layers.

#include "sdk/core/engine.hpp"
#include "sdk/core/logging.hpp"
#include "sdk/hooking/hook.hpp"
#include "sdk/overlay/overlay.hpp"
#include "sdk/scripting/callback_registry.hpp"

#include <safetyhook.hpp>
#include <spdlog/spdlog.h>

#include <windows.h>

namespace sdk::core
{

// ─── engine::impl ────────────────────────────────────────────────────────

struct engine::impl
{
    config                                 cfg;
    bool                                   initialized = false;
    std::atomic<render_api>                detected_api{ render_api::unknown };
    std::atomic<bool>                      overlay_available{ false };
    std::atomic<bool>                      overlay_visible{ true };
    
    platform::window_handle*               window = nullptr;
    
    std::unique_ptr<scripting::state>      scripting_state;
    std::unique_ptr<scripting::callback_registry> callbacks;
    std::unique_ptr<hooking::hook_registry> hooks;
    std::unique_ptr<overlay::manager>      overlay_mgr;
    
    // Hook instances
    hooking::inline_hook ll_a_hook;
    hooking::inline_hook ll_w_hook;

    impl(config c) : cfg(std::move(c))
    {
        scripting_state = scripting::state::create();
        callbacks       = std::make_unique<scripting::callback_registry>();
        hooks           = std::make_unique<hooking::hook_registry>();
        overlay_mgr     = std::make_unique<overlay::manager>();
    }
};

// ─── engine implementation ───────────────────────────────────────────────

engine::engine(config cfg) 
    : pimpl_(std::make_unique<impl>(std::move(cfg)))
{
}

engine::~engine()
{
    if (pimpl_ && pimpl_->initialized)
    {
        shutdown();
    }
}

engine::engine(engine&&) noexcept = default;
auto engine::operator=(engine&&) noexcept -> engine& = default;

void engine::init()
{
    if (pimpl_->initialized)
    {
        spdlog::warn("[engine] already initialized");
        return;
    }

    logging::init(pimpl_->cfg.log_dir.string(), pimpl_->cfg.log_level);
    spdlog::info("[engine] initializing with log_dir={}, plugin_dir={}",
                 pimpl_->cfg.log_dir.string(),
                 pimpl_->cfg.plugin_dir.string());

    pimpl_->initialized = true;
    spdlog::info("[engine] initialized");
}

void engine::shutdown()
{
    if (!pimpl_->initialized)
    {
        return;
    }

    spdlog::info("[engine] shutting down");

    uninstall_hooks();
    unload_plugins();

    if (pimpl_->overlay_available.load(std::memory_order_acquire))
    {
        pimpl_->overlay_mgr->shutdown();
    }

    pimpl_->initialized = false;
    spdlog::info("[engine] shutdown complete");
    logging::shutdown();
}

auto engine::is_initialized() const noexcept -> bool
{
    return pimpl_->initialized;
}

void engine::install_hooks()
{
    spdlog::info("[engine] installing hooks");

    // Hook LoadLibraryA
    pimpl_->ll_a_hook = hooking::inline_hook::create(
        GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryA"),
        nullptr  // TODO: detour function
    );

    // Hook LoadLibraryW
    pimpl_->ll_w_hook = hooking::inline_hook::create(
        GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW"),
        nullptr  // TODO: detour function
    );

    // Hook OpenGL functions
    pimpl_->hooks->install(L"opengl32.dll", "wglSwapBuffers", nullptr);
    pimpl_->hooks->install(L"opengl32.dll", "glMatrixMode", nullptr);
    pimpl_->hooks->install(L"opengl32.dll", "glLoadIdentity", nullptr);
    pimpl_->hooks->install(L"glu32.dll", "gluLookAt", nullptr);

    spdlog::info("[engine] hooks installed");
}

void engine::uninstall_hooks()
{
    spdlog::info("[engine] uninstalling hooks");

    pimpl_->ll_a_hook.reset();
    pimpl_->ll_w_hook.reset();
    pimpl_->hooks->reset_all();

    spdlog::info("[engine] hooks uninstalled");
}

auto engine::get_render_api() const noexcept -> render_api
{
    return pimpl_->detected_api.load(std::memory_order_acquire);
}

auto engine::is_overlay_available() const noexcept -> bool
{
    return pimpl_->overlay_available.load(std::memory_order_acquire);
}

void engine::load_plugins()
{
    if (!pimpl_->cfg.enable_scripting)
    {
        spdlog::debug("[engine] scripting disabled");
        return;
    }

    if (!std::filesystem::exists(pimpl_->cfg.plugin_dir))
    {
        spdlog::warn("[engine] plugin directory not found: {}", pimpl_->cfg.plugin_dir.string());
        std::filesystem::create_directories(pimpl_->cfg.plugin_dir);
        return;
    }

    spdlog::info("[engine] loading plugins from {}", pimpl_->cfg.plugin_dir.string());

    for (const auto& entry : std::filesystem::directory_iterator(pimpl_->cfg.plugin_dir))
    {
        if (entry.path().extension() == ".lua")
        {
            auto result = pimpl_->scripting_state->execute_file(entry.path());
            if (result)
            {
                spdlog::info("[engine] loaded plugin: {}", entry.path().filename().string());
            }
            else
            {
                spdlog::error("[engine] failed to load plugin {}: {}", 
                            entry.path().filename().string(),
                            result.error().message);
            }
        }
    }

    pimpl_->scripting_state->invoke_callbacks("on_load");
    spdlog::info("[engine] plugins loaded");
}

void engine::unload_plugins()
{
    if (!pimpl_->cfg.enable_scripting || !pimpl_->scripting_state)
    {
        return;
    }

    pimpl_->scripting_state->invoke_callbacks("on_unload");
    pimpl_->scripting_state->clear_all_callbacks();
    pimpl_->callbacks->clear_all();

    spdlog::info("[engine] plugins unloaded");
}

auto engine::get_scripting_state() -> scripting::state*
{
    return pimpl_->scripting_state.get();
}

void engine::toggle_overlay() noexcept
{
    pimpl_->overlay_visible = !pimpl_->overlay_visible.load();
}

auto engine::is_overlay_visible() const noexcept -> bool
{
    return pimpl_->overlay_visible.load();
}

void engine::set_window(platform::window_handle* hwnd)
{
    pimpl_->window = hwnd;
}

auto engine::get_window() const noexcept -> platform::window_handle*
{
    return pimpl_->window;
}

void engine::on_frame()
{
    if (pimpl_->scripting_state)
    {
        pimpl_->scripting_state->invoke_callbacks("on_frame");
    }
}

void engine::on_overlay()
{
    if (!pimpl_->overlay_visible.load())
    {
        return;
    }

    if (pimpl_->scripting_state)
    {
        pimpl_->scripting_state->invoke_callbacks("on_overlay");
    }

    if (pimpl_->overlay_available.load())
    {
        pimpl_->overlay_mgr->render();
    }
}

} // namespace sdk::core
