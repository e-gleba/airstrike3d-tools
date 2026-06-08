/// @file detail/register_bindings.hpp
/// @brief Entry point for registering all binding schemas.
///
/// Private header — called only from the plugin loader.

#pragma once

namespace sdk::lua
{

class LuaState;

namespace detail
{

/// Register every binding schema (sdk, ui, math, constants) into @p vm.
/// This is the **only** function that bridges binding schemas to the
/// backend — swap this single translation unit to change backends.
void register_all(LuaState& vm);

} // namespace detail
} // namespace sdk::lua
