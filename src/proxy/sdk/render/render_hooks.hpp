/// @file render/render_hooks.hpp
/// @brief Public RAII interface for the render hook subsystem.
///
/// All implementation details (safetyhook, OpenGL/DirectX types,
/// imgui) are hidden behind the pimpl idiom in detail/.

#pragma once

#include <cstdint>
#include <functional>
#include <memory>

namespace sdk::render
{

/// Detected render API.
enum class api : uint8_t
{
    unknown,
    opengl,
    directx
};

/// RAII render hook subsystem.
///
/// Owns:
/// - OpenGL function hooks (glMatrixMode, glLoadIdentity, gluLookAt)
/// - DirectX DLL detection (LoadLibrary hooks)
/// - Swap buffer hook (wglSwapBuffers)
/// - ImGui overlay lifecycle
/// - Window procedure hook
///
/// Thread-safety: all state is atomic or mutex-protected.
/// Callbacks may be registered from any thread.
class HookSystem
{
public:
    HookSystem();
    ~HookSystem();

    HookSystem(const HookSystem&)            = delete;
    HookSystem& operator=(const HookSystem&) = delete;
    HookSystem(HookSystem&&) noexcept;
    HookSystem& operator=(HookSystem&&) noexcept;

    /// Install all render hooks and begin API detection.
    void install();

    /// Uninstall hooks, shut down overlay, restore wndproc.
    void uninstall();

    /// Currently detected render API.
    [[nodiscard]] api detected_api() const noexcept;

    /// Whether overlay rendering is available (OpenGL only currently).
    [[nodiscard]] bool overlay_available() const noexcept;

    // ── Callback types ──────────────────────────────────────────────────
    // All type-erased — no GL, DirectX, or imgui types exposed.

    using void_fn     = std::function<void()>;
    using key_fn      = std::function<bool(int)>;
    using identity_fn = std::function<void(uint32_t)>;
    using lookat_fn   = std::function<bool(double, double, double,
                                           double, double, double,
                                           double, double, double)>;

    /// Register a callback invoked each frame (before overlay render).
    void on_frame(void_fn fn);

    /// Register a callback invoked during overlay render pass.
    void on_overlay(void_fn fn);

    /// Register a callback for key-down events. Return true to consume.
    void on_key_down(key_fn fn);

    /// Register a callback for glLoadIdentity in MODELVIEW mode.
    void on_gl_identity(identity_fn fn);

    /// Register a callback for gluLookAt. Return true to consume.
    void on_glu_lookat(lookat_fn fn);

    /// Clear all registered callbacks.
    void clear_callbacks();

    /// Call original gluLookAt trampoline (for Lua gl_apply_lookat).
    void call_orig_glu_lookat(double ex, double ey, double ez,
                               double cx, double cy, double cz,
                               double ux, double uy, double uz);

private:
    struct impl;
    std::unique_ptr<impl> pimpl_;
};

} // namespace sdk::render
