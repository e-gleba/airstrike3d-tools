#pragma once

#include <sol/forward.hpp>

namespace sdk::lua::detail
{
struct callback_registry;
}

namespace sdk::lua::bindings
{

/// @brief Register SDK core bindings (callbacks, GL, input, logging).
void register_sdk(sol::state& state, detail::callback_registry& callbacks);

/// @brief Register Dear ImGui UI bindings.
void register_ui(sol::state& state);

/// @brief Register math bindings (vectors, matrices, trigonometry).
void register_math(sol::state& state);

/// @brief Register constant bindings (virtual keys, GL constants).
void register_constants(sol::state& state);

} // namespace sdk::lua::bindings
