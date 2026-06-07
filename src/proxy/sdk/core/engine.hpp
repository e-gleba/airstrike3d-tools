#pragma once

// sdk/core/engine.hpp — RAII engine class (pure C++23 interface)
//
// Polukhin-style: PIMPL, ABI-stable.
// Turner-style: RAII, noexcept where possible.
// Stefano-style: Concept-constrained configuration.
//
// This replaces the global g_ctx with proper RAII lifecycle management.
// Now includes renderer abstraction for OpenGL/DirectX8 support.

#include "sdk/api/render_api.hpp"
#include "sdk/platform/types.hpp"
#include "sdk/scripting/state.hpp"
#include "sdk/render/renderer.hpp"

#include <atomic>
#include <filesystem>
#include <memory>
#include <string_view>

namespace sdk::core
{

// ─── Engine configuration (concept-constrained) ──────────────────────────

template<typename T>
concept valid_log_level = requires(T level) {
    { level == "trace" } || { level == "debug" } || { level == "info" } || 
    { level == "warn" } || { level == "error" } || { level == "critical" };
};

struct config
{
    std::filesystem::path log_dir         = "logs";
    std::filesystem::path plugin_dir      = "plugins";
    std::string_view      log_level       = "info";
    bool                  enable_overlay  = true;
    bool                  enable_scripting = true;
};

// ─── Engine class (RAII) ─────────────────────────────────────────────────

class engine
{
public:
    // Construct with configuration
    explicit engine(config cfg = {});
    
    // Destructor - automatic cleanup
    ~engine();

    // Non-copyable
    engine(engine const&) = delete;
    auto operator=(engine const&) -> engine& = delete;

    // Movable
    engine(engine&&) noexcept;
    auto operator=(engine&&) noexcept -> engine&;

    // ─── Lifecycle ──────────────────────────────────────────────────────

    // Initialize engine (logging, renderer, overlay, scripting)
    void init();

    // Shutdown engine (reverse order)
    void shutdown();

    // Check if initialized
    [[nodiscard]] auto is_initialized() const noexcept -> bool;

    // ─── Hooks ──────────────────────────────────────────────────────────

    // Install all hooks (LoadLibrary, OpenGL/DirectX, etc.)
    void install_hooks();

    // Uninstall all hooks
    void uninstall_hooks();

    // ─── Render API detection ───────────────────────────────────────────

    // Get detected render API
    [[nodiscard]] auto get_render_api() const noexcept -> render_api;

    // Check if overlay is available (OpenGL detected)
    [[nodiscard]] auto is_overlay_available() const noexcept -> bool;

    // ─── Renderer ───────────────────────────────────────────────────────

    // Get renderer instance (OpenGL or DirectX8 based on detected API)
    [[nodiscard]] auto get_renderer() -> render::renderer*;

    // ─── Scripting ──────────────────────────────────────────────────────

    // Load all plugins from plugin directory
    void load_plugins();

    // Unload all plugins
    void unload_plugins();

    // Get scripting state
    [[nodiscard]] auto get_scripting_state() -> scripting::state*;

    // ─── UI ─────────────────────────────────────────────────────────────

    // Toggle overlay visibility
    void toggle_overlay() noexcept;

    // Check if overlay is visible
    [[nodiscard]] auto is_overlay_visible() const noexcept -> bool;

    // ─── Window ─────────────────────────────────────────────────────────

    // Set window handle (for overlay initialization)
    void set_window(platform::window_handle* hwnd);

    // Get window handle
    [[nodiscard]] auto get_window() const noexcept -> platform::window_handle*;

    // ─── Frame callbacks ────────────────────────────────────────────────

    // Called on each frame (for user plugins)
    void on_frame();

    // Called when overlay should render
    void on_overlay();

private:
    struct impl;
    std::unique_ptr<impl> pimpl_;
};

} // namespace sdk::core
