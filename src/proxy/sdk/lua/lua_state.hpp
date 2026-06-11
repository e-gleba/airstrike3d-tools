/// @file lua_state.hpp
/// @brief Public RAII wrapper for Lua execution context.
///
/// This header contains **no sol2 types** — all backend-specific
/// implementation is hidden behind the pimpl idiom in detail/.

#pragma once

#include <memory>

namespace sdk::render { class HookSystem; }

namespace sdk::lua
{

/// RAII Lua interpreter.
///
/// Owns the Lua state and manages its lifetime. When destroyed,
/// the interpreter is torn down cleanly.
///
/// Thread-safety: callback lists in g_ctx.cb use their own mutex.
/// The Lua state itself should only be accessed from one thread at a time.
class LuaState
{
public:
    /// Initialize the Lua interpreter.
    /// Opens standard libraries, registers C++ bindings, and sets up
    /// callback hooks (hook_frame, hook_overlay, hook_key_down, etc.).
    ///
    /// @param render Reference to the render hook subsystem for callback registration.
    explicit LuaState(render::HookSystem& render);

    /// Shut down the Lua interpreter.
    /// Clears all callbacks and releases resources.
    ~LuaState();

    // Non-copyable, movable
    LuaState(const LuaState&) = delete;
    LuaState& operator=(const LuaState&) = delete;
    LuaState(LuaState&&) noexcept;
    LuaState& operator=(LuaState&&) noexcept;

    /// Load all .lua scripts from the plugins/ directory.
    /// Calls on_load callback after loading.
    void load_plugins();

    /// Unload all plugins.
    /// Calls on_unload callback and clears all registered callbacks.
    void unload_plugins();

private:
    struct impl;
    std::unique_ptr<impl> pimpl;
};

} // namespace sdk::lua
