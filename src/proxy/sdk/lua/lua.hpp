/// @file lua.hpp
/// @brief Umbrella header for the SDK Lua subsystem.
///
/// This is the **only** header that consumers should include.
/// It transitively provides the RAII state wrapper and the
/// callback manager — both free of sol2 / raw-Lua types.
///
/// @note To replace the Lua backend, modify files under `detail/` only.

#pragma once

#include "lua/callback_manager.hpp"
#include "lua/lua_state.hpp"
