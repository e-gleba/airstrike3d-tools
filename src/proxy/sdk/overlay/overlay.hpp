#pragma once

// sdk/overlay/overlay.hpp — Overlay interface (pure C++23)
//
// Polukhin-style: PIMPL, no imgui in public header.
// Turner-style: RAII, noexcept where possible.

#include "sdk/platform/types.hpp"
#include "sdk/render/types.hpp"

#include <memory>
#include <string_view>

namespace sdk::overlay
{

// ─── Overlay manager (RAII) ──────────────────────────────────────────────

class manager
{
public:
    struct config
    {
        std::string_view glsl_version = "#version 110";
        bool             auto_show    = true;
    };

    manager();
    ~manager();

    manager(manager const&)            = delete;
    auto operator=(manager const&) -> manager& = delete;

    manager(manager&&) noexcept;
    auto operator=(manager&&) noexcept -> manager&;

    // Initialize overlay with device context
    void init(render::device_context* ctx, config cfg = {});

    // Render current frame
    void render();

    // Shutdown overlay
    void shutdown();

    // Toggle visibility
    void toggle_visibility() noexcept;

    // Check if visible
    [[nodiscard]] auto is_visible() const noexcept -> bool;

    // Process window message (returns true if handled)
    auto process_message(std::uint32_t msg, std::uintptr_t wparam, std::intptr_t lparam) -> bool;

private:
    struct impl;
    std::unique_ptr<impl> pimpl_;
};

// ─── Window procedure handler ───────────────────────────────────────────

// Custom WndProc that integrates overlay input handling
[[nodiscard]] auto wnd_proc_handler(platform::window_handle* hwnd,
                                    std::uint32_t msg,
                                    std::uintptr_t wparam,
                                    std::intptr_t lparam,
                                    manager* overlay) -> bool;

} // namespace sdk::overlay
