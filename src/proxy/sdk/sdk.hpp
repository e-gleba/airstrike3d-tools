#pragma once

/// @file sdk.hpp
/// @brief Public SDK interface for Airstrike 3D modding.
///
/// This header provides the complete public API for registering callbacks.
/// Implementation details (Lua, OpenGL, etc.) are hidden.
///
/// @code
/// #include <sdk/sdk.hpp>
///
/// void my_plugin() {
///     sdk::on_frame([] {
///         // called every frame
///     });
///
///     sdk::on_key_down([](int vk) -> bool {
///         if (vk == VK_F1) { /* handle */ return true; }
///         return false;
///     });
/// }
/// @endcode
///
/// @note Thread-safe. All functions acquire an internal mutex.
/// @note Consuming callbacks (on_key_down, on_glu_lookat) return bool.
///       Return true to prevent further processing.

#include <functional>

namespace sdk {

// ─── Frame callbacks ─────────────────────────────────────────────────────────

/// Register a callback invoked every frame during rendering.
void on_frame(std::function<void()> callback);

/// Register a callback invoked during ImGui overlay rendering.
void on_overlay(std::function<void()> callback);

/// Register a callback invoked when glLoadIdentity is called in MODELVIEW mode.
void on_gl_identity(std::function<void()> callback);

// ─── Consuming callbacks ─────────────────────────────────────────────────────

/// Register a callback invoked when gluLookAt is called.
/// Return true to consume the event (prevent original gluLookAt call).
void on_glu_lookat(
    std::function<bool(double, double, double, double, double, double, double, double, double)>
        callback);

/// Register a callback invoked when a key is pressed.
/// Return true to consume the event (prevent game from seeing the keypress).
void on_key_down(std::function<bool(int)> callback);

// ─── Lifecycle callbacks ─────────────────────────────────────────────────────

/// Register a callback invoked after all plugins are loaded.
void on_load(std::function<void()> callback);

/// Register a callback invoked before plugins are unloaded.
void on_unload(std::function<void()> callback);

} // namespace sdk
