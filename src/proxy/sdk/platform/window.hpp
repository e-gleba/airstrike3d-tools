#pragma once

// sdk/platform/window.hpp — RAII window subclassing (pure C++23 interface)
//
// Design: Polukhin-style RAII, no windows.h in public header.

#include "sdk/platform/types.hpp"

#include <functional>
#include <memory>

namespace sdk::platform
{

// Window procedure callback signature
using wnd_proc_fn = std::function<bool(std::uint32_t msg, std::uintptr_t wparam, std::intptr_t lparam)>;

// RAII window subclasser — installs custom WndProc, restores on destruction
class window_subclass
{
public:
    // Construct with opaque window handle and custom message handler
    explicit window_subclass(window_handle* hwnd, wnd_proc_fn handler);
    ~window_subclass();

    window_subclass(window_subclass const&)            = delete;
    auto operator=(window_subclass const&) -> window_subclass& = delete;

    window_subclass(window_subclass&&) noexcept;
    auto operator=(window_subclass&&) noexcept -> window_subclass&;

private:
    struct impl;
    std::unique_ptr<impl> pimpl_;
};

// Query window rectangle
[[nodiscard]] auto get_window_rect(window_handle* hwnd) noexcept -> rect;

} // namespace sdk::platform
