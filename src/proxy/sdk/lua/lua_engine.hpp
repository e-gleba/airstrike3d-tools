#pragma once

#include <sol/state.hpp>

namespace sdk::lua
{

void register_sdk_bindings(sol::state&);
void register_ui_bindings(sol::state&);
void register_math_bindings(sol::state&);
void register_constant_bindings(sol::state&);

void load_plugins();
void unload_plugins();

} // namespace sdk::lua