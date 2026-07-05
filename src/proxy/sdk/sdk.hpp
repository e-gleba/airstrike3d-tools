/// @file sdk.hpp
/// @brief Public SDK API — single header for SDK consumers.
///
/// This is the main entry point for SDK users. It provides:
/// - Hook management (install/uninstall)
/// - Scripting engine interface
/// - Clean, backend-agnostic types
///
/// Implementation details (Lua, ImGui, SafetyHook) are hidden.

#pragma once

#include "sdk/core/context.hpp"
#include "sdk/core/types.hpp"
#include "sdk/scripting/engine.hpp"

namespace sdk
{

/// Install all hooks and initialize the scripting engine.
///
/// This function:
/// - Detects the rendering API (OpenGL/DirectX)
/// - Installs OpenGL hooks (wglSwapBuffers, glMatrixMode, etc.)
/// - Hooks LoadLibrary to detect DirectX DLLs
/// - Initializes the scripting engine (Lua backend)
/// - Loads plugins from the plugins/ directory
///
/// @throws std::runtime_error if initialization fails
void install_hooks();

/// Uninstall all hooks and shut down the scripting engine.
///
/// This function:
/// - Unloads all plugins (calls on_unload callbacks)
/// - Shuts down the scripting engine
/// - Removes all hooks
/// - Restores original window procedure
void uninstall_hooks();

/// Get the global context (advanced usage).
///
/// Most users don't need direct access to the context.
/// Use the scripting API or callback system instead.
[[nodiscard]] context& get_context() noexcept;

} // namespace sdk
