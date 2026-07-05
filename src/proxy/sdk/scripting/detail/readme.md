# Scripting Backends (`detail/`)

The `scripting/detail/` directory contains **mutually exclusive** backend
implementations of `sdk::scripting::engine`. Only one backend is compiled
per build, selected by the `SDK_EXPERIMENTAL_LUABRIDGE3` CMake option.

## Files

| File                   | Backend    | Selected when                        |
| ---------------------- | ---------- | ------------------------------------ |
| `engine_lua.cpp`       | sol2       | `SDK_EXPERIMENTAL_LUABRIDGE3=OFF`    |
| `engine_luabridge3.cpp`| LuaBridge3 | `SDK_EXPERIMENTAL_LUABRIDGE3=ON`     |

Both backends expose the **same** Lua API surface to plugin scripts.
Downstream code and Lua plugins do **not** need modification when switching
backends.

## Backend Comparison

### sol2 (default)

- Heavy template metaprogramming; slower compile times
- Rich API (`sol::state`, `sol::protected_function`, `sol::table`)
- Uses exceptions heavily for error paths
- Header: `<sol/sol.hpp>`

### LuaBridge3 (experimental)

- Header-only, dependency-free, MIT-licensed ([kunitoki/LuaBridge3](https://github.com/kunitoki/LuaBridge3))
- 2.6× faster on Lua→C++ calls (Increment benchmark: 85ms vs 225ms)
- Leaner compile times; linear scaling with binding surface
- Raw `lua_State*` with `luabridge::LuaRef` / `TypeResult<T>` for errors
- Headers: `<LuaBridge/LuaBridge.h>` + `<lua.h>`

## API Surface

Both backends register these Lua namespaces:

- **`gmath`** — math utilities (`radians`, `cos`, `sin`, `normalize`, `cross`, `lookat_matrix`)
- **`VK`** — virtual key constants (e.g. `VK.SHIFT`, `VK.F1`)
- **`GL`** — OpenGL constants (e.g. `GL.MODELVIEW`, `GL.TRIANGLES`)
- **`sdk`** — graphics, platform, callback registration (`sdk.on_frame`, `sdk.gl_enable`, `sdk.is_key_down`)
- **`ui`** — ImGui bindings (`ui.begin_window`, `ui.checkbox`, `ui.slider_float`)

### Callback Registration Pattern

```lua
-- Same API regardless of backend
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
    struct impl;                      // defined in detail/engine_*.cpp
    std::unique_ptr<impl> pimpl_;
public:
    engine();
    void load_plugins();
    void unload_plugins();
};
```

To add new Lua bindings:
1. Add the C++ function in both `engine_lua.cpp` AND `engine_luabridge3.cpp`
2. Use each library's API style — see existing `register_sdk_bindings()` for examples
3. Keep function signatures identical across backends

## Build Instructions

```bash
# Default (sol2)
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Experimental (LuaBridge3)
cmake -B build -DCMAKE_BUILD_TYPE=Release -DSDK_EXPERIMENTAL_LUABRIDGE3=ON
cmake --build build
```

## Error Handling Differences

| Operation          | sol2                                          | LuaBridge3                              |
| ------------------ | --------------------------------------------- | --------------------------------------- |
| Bad Lua call       | `sol::protected_function` → `sol::error`      | `result` falsy → `result.message()`     |
| Type mismatch      | throws or `sol::type` check                   | `TypeResult<T>` → `cast<T>()` check     |
| Script load error  | `lua.safe_script_file()` returns invalid      | `luaL_loadfile` + `lua_pcall` check     |

Both backends log errors via `sdk::log_error()` and continue loading remaining plugins.

## Thread Safety

Same as sol2 backend: callback lists use `g_ctx.scripting_mutex` (see
`callback.hpp`). Engine must be accessed from a single thread at runtime.

## Migration Checklist

To fully migrate from sol2 to LuaBridge3:

1. [ ] Build with `SDK_EXPERIMENTAL_LUABRIDGE3=ON`
2. [ ] Run full test suite (CTest)
3. [ ] Run all plugin scripts manually, verify output parity
4. [ ] Benchmark hot paths (Increment, function calls, table access)
5. [ ] Compare compile times and binary sizes
6. [ ] Remove `engine_lua.cpp` and sol2 dependency once parity verified
7. [ ] Rename `engine_luabridge3.cpp` → `engine_lua.cpp`

See issue [#52](https://github.com/e-gleba/airstrike3d-tools/issues/52) for full context.
