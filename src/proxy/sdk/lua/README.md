# Lua Scripting Engine

Modern C++23 Lua integration with clean architecture and strong separation of concerns.

## Architecture

```
lua/
├── public/              # Public API (no sol2 exposure)
│   └── lua_engine.hpp   # Clean C++23 interface
├── private/             # Private implementation (sol2 isolated here)
│   ├── lua_state.*      # RAII wrapper for sol2 state
│   └── callback_registry.* # Type-erased callback management
├── bindings/            # Lua binding modules (organized by domain)
│   ├── bindings.hpp     # Binding interface
│   ├── constants.cpp    # VK_*, GL_* constants
│   ├── math.cpp         # Vector/matrix operations
│   ├── sdk.cpp          # SDK callbacks, GL, input, logging
│   └── ui.cpp           # ImGui wrappers
└── lua_engine.cpp       # Public API implementation
```

## Design Principles

1. **Clean Public API**: No sol2 or Lua types exposed to clients
2. **RAII**: Automatic lifetime management, no manual cleanup
3. **Type Erasure**: `std::function` callbacks hide implementation details
4. **Modular Bindings**: Each domain (math, UI, SDK) in separate module
5. **Thread Safety**: All operations are thread-safe via internal mutexes
6. **Error Handling**: Structured error reporting, no exceptions in public API
7. **Modern C++23**: Uses ranges, concepts, const correctness, [[nodiscard]]

## Usage

```cpp
#include "lua/public/lua_engine.hpp"

// Create engine with default configuration
sdk::lua::engine eng;

// Load all plugins from "plugins" directory
auto results = eng.load_plugins();

// Register C++ callbacks
eng.register_callback("on_frame", []() {
    // Called every frame from Lua scripts
});

// Invoke callbacks
eng.invoke_callback("on_frame");

// Automatic cleanup on destruction
```

## Configuration

```cpp
sdk::lua::engine_config cfg;
cfg.plugin_directory = "scripts";
cfg.auto_create_plugin_dir = true;
cfg.enable_standard_libs = true;

sdk::lua::engine eng{cfg};
```

## Lua API

Scripts have access to these namespaces:

- `sdk.*` - Core SDK functions (callbacks, GL, input, logging)
- `ui.*` - ImGui UI functions
- `gmath.*` - Math operations (vectors, matrices)
- `VK.*` - Virtual key constants
- `GL.*` - OpenGL constants

See `bindings/*.cpp` for complete API reference.

## Backend Swap

To swap sol2 for another backend (e.g., LuaBridge3):

1. Implement `detail::lua_state` using new backend
2. Update `detail::callback_registry` to use new callback types
3. Update `bindings/*.cpp` to use new binding syntax
4. **Public API remains unchanged**

This isolation enables future-proof architecture.

## Thread Safety

- **Safe**: Concurrent callback invocation from multiple threads
- **Safe**: Concurrent read operations (has_callbacks, callback_count)
- **Unsafe**: Concurrent load_plugins/unload_plugins (must be externally synchronized)

## Error Handling

All script execution errors are captured in `script_result`:

```cpp
auto result = eng.load_script("broken.lua");
if (!result.success) {
    spdlog::error("Failed to load {}: {}", 
                  result.script_path, 
                  result.error_message);
}
```

No exceptions escape the public API.
