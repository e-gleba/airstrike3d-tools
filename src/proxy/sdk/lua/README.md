# Lua Scripting Engine — Clean Architecture

Modern C++23 Lua integration with complete sol2 isolation via pimpl idiom.

## Directory Structure

```
lua/
├── lua_engine.hpp                  ← Public API (zero sol2 exposure)
├── detail/
│   ├── callback.hpp                ← Thread-safe callback_list + callback_registry
│   ├── lua_engine_impl.hpp         ← engine::impl definition (owns sol::state)
│   └── lua_engine_impl.cpp         ← Full engine implementation
└── bindings/
    ├── bindings_fwd.hpp            ← Forward declarations (sol::state only)
    ├── sdk_bindings.cpp            ← SDK callbacks, GL wrappers, input, logging
    ├── ui_bindings.cpp             ← Dear ImGui wrappers
    ├── math_bindings.cpp           ← Vector/matrix/trig (glm)
    └── constant_bindings.cpp       ← VK_* and GL_* constants
```

## Key Design Decisions

### sol2 Isolation

`lua_engine.hpp` includes only `<memory>`. Zero sol2 headers leak to consumers.
All sol2 types (`sol::state`, `sol::protected_function`, `sol::table`) live
exclusively in `detail/` and `bindings/`.

### Pimpl Idiom

```
lua_engine.hpp          → class engine { unique_ptr<impl> pimpl_; }
detail/lua_engine_impl.hpp → struct engine::impl { sol::state lua; callback_registry callbacks; }
detail/lua_engine_impl.cpp → engine::engine(), load_plugins(), invoke_on_frame(), etc.
```

Swap sol2 → LuaBridge3 by rewriting only `detail/` and `bindings/`. Public API unchanged.

### Callback Decoupling

Old: bindings accessed `g_ctx.cb` (global context dependency).
New: `register_sdk(sol::state&, callback_registry&)` — registry injected.

### RAII Lifetime

```cpp
// Creation (install_hooks)
g_ctx.lua_engine = std::make_unique<lua::engine>();
g_ctx.lua_engine->load_plugins();

// Destruction (uninstall_hooks)
g_ctx.lua_engine->unload_plugins();
g_ctx.lua_engine.reset();
```

No manual `lua_close()`, no mutex management by caller.

### Destruction Order

`engine::impl` members declared in critical order:
1. `sol::state lua` — destroyed last (reverse declaration order)
2. `detail::callback_registry callbacks` — destroyed first

Ensures `sol::protected_function` objects (which reference the Lua state)
are destroyed before the state itself.

## Public API

```cpp
namespace sdk::lua {

class engine {
public:
    engine();                          // Creates sol::state, registers all bindings
    ~engine();                         // Default (sol::state auto-cleanup)
    engine(engine&&) noexcept;         // Move-only

    void load_plugins();               // Scan plugins/ dir, execute .lua scripts
    void unload_plugins();             // Fire on_unload, clear all callbacks

    void invoke_on_frame();            // Fire-and-forget callbacks
    void invoke_on_overlay();
    void invoke_on_gl_identity();
    void invoke_on_glu_lookat();
    [[nodiscard]] bool invoke_on_key_down(int key);  // Consuming callback
    void invoke_on_load();
    void invoke_on_unload();

    [[nodiscard]] bool has_plugins() const;
};

} // namespace sdk::lua
```

## Thread Safety

- `callback_list` uses `std::recursive_mutex` (supports reentrant invocation)
- All invoke methods are null-safe (check `pimpl_` before dereference)
- Plugin loading is not thread-safe (call during init only)

## Context Integration

`context.hpp` no longer includes sol2:

```cpp
#include "sdk/lua/lua_engine.hpp"  // Only standard C++ types

struct context {
    // ...
    std::unique_ptr<lua::engine> lua_engine;  // Replaces: unique_ptr<sol::state> + callback_lists
    // ...
};
```

Removed: `sol::state`, `lua_mutex`, all `callback_list` members, `clear_callbacks()`.
