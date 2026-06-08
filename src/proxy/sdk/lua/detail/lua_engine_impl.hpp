#pragma once

#include "sdk/lua/detail/callback.hpp"
#include "sdk/lua/lua_engine.hpp"

#include <sol/state.hpp>

namespace sdk::lua
{

/// @brief Private implementation of the Lua engine.
///
/// Owns the sol::state and all callback lists.  Created once during
/// engine construction; destroyed with the engine.  Destruction order
/// matters: callbacks_ must be destroyed before lua_ because
/// sol::protected_function objects reference the Lua state.
struct engine::impl final
{
    sol::state                lua;
    detail::callback_registry callbacks;
    bool                      plugins_loaded{ false };

    /// @brief Open standard libraries and register all C++ bindings.
    impl();

    /// @brief Scan plugins/ directory and execute .lua scripts.
    void load_plugins_from_directory();

    /// @brief Fire on_unload, clear all callbacks, mark unloaded.
    void invoke_on_unload_and_clear();
};

} // namespace sdk::lua
