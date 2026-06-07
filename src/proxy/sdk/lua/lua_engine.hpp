#pragma once

namespace sdk::lua
{

/// Load all plugins from plugins/ directory.
/// Initializes script engine, registers bindings, executes .lua files.
void load_plugins();

/// Unload all plugins and destroy script engine.
void unload_plugins();

} // namespace sdk::lua
