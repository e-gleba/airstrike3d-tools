# Scripting Engine (`detail/`)

The `scripting/detail/` directory contains the LuaBridge3 backend implementation
of `sdk::scripting::engine`.

## Files

| File             | Purpose                                    |
| ---------------- | ------------------------------------------ |
| `engine_lua.cpp` | LuaBridge3 backend implementation          |

## Lua API Surface

The engine registers these Lua namespaces:

- **`gmath`** — math utilities (`radians`, `cos`, `sin`, `normalize`, `cross`, `lookat_matrix`)
- **`VK`** — virtual key constants (e.g. `VK.SHIFT`, `VK.F1`)
- **`GL`** — OpenGL constants (e.g. `GL.MODELVIEW`, `GL.TRIANGLES`)
- **`sdk`** — graphics, platform, callback registration (`sdk.on_frame`, `sdk.gl_enable`, `sdk.is_key_down`)
- **`ui`** — ImGui bindings (`ui.begin_window`, `ui.checkbox`, `ui.slider_float`)

### Callback Registration

```lua
sdk.on_frame(function()
    -- runs every frame
end)

sdk.on_key_down(function(key)
    if key == VK.INSERT then
        return true  -- consumed
    end
    return false
end)
```

## C++ Interface

The pimpl boundary in `engine.hpp` isolates backend internals:

```cpp
class engine final {
    struct impl;                      // defined in detail/engine_lua.cpp
    std::unique_ptr<impl> pimpl_;
public:
    engine();
    void load_plugins();
    void unload_plugins();
};
```

## Adding New Bindings

To add new Lua bindings:
1. Add the C++ function in `engine_lua.cpp`
2. Use LuaBridge3's `addFunction()` API in the appropriate `register_*()` method
3. For constexpr constants, use raw Lua C API (see `register_constants()`)

## Error Handling

- Script load errors: `luaL_loadfile` + `lua_pcall` check, logged via `sdk::log_error()`
- Callback errors: `result` falsy → `result.message()` logged
- Type mismatches: `TypeResult<T>` → `cast<T>()` check with `value_or()` fallback

Errors are logged and execution continues (plugin loading is fault-tolerant).

## Thread Safety

Callback lists use `g_ctx.scripting_mutex` (see `callback.hpp`).
Engine must be accessed from a single thread at runtime.

## Performance

LuaBridge3 provides 2.6× faster Lua→C++ calls compared to the previous sol2 backend:
- Increment benchmark: 85ms vs 225ms (100k calls)
- Leaner compile times due to simpler template metaprogramming
- Smaller binary footprint

See issue [#52](https://github.com/e-gleba/airstrike3d-tools/issues/52) for migration context.
