# Airstrike 3D Tools — Lua Plugins

Professional Lua plugin system for game modification and analysis.

## Architecture

- **Framework**: `_ui_framework.lua` provides the core UI system
- **Plugins**: Individual `.lua` files that register with the framework
- **Loading Order**: Files prefixed with `_` load first (framework files)
- **Hot Reload**: Use `[INSERT]` → `Unload` button to reload plugins

## Plugin Structure

```lua
--- Plugin metadata
---@meta
--- Description
--- @module plugin_name
--- @version 1.0.0

local M = {}

-- Configuration
local cfg = {
    enabled = true,
    -- ...
}

-- Hooks
sdk.on_frame(function()
    -- Frame update logic
end)

sdk.on_key_down(function(vk)
    -- Key handling
    return false -- not consumed
end)

-- UI Panel
local function draw_panel()
    TOOLS_UI.header("Plugin Name")
    TOOLS_UI.checkbox("Enable", cfg, "enabled", "Tooltip")
end

-- Registration
if _G.TOOLS_UI then
    TOOLS_UI.register_panel("plugin_id", "Plugin Name", draw_panel)
end

-- Lifecycle
sdk.on_load(function()
    sdk.log_info("Plugin loaded")
end)

sdk.on_unload(function()
    sdk.log_info("Plugin unloaded")
end)

return M
```

## Available Hooks

### Frame Hooks
- `sdk.on_frame(fn)` — Called every frame
- `sdk.on_overlay(fn)` — Called during overlay rendering

### Input Hooks
- `sdk.on_key_down(fn(vk))` — Key press, return `true` to consume
- `sdk.on_key_up(fn(vk))` — Key release

### OpenGL Hooks
- `sdk.on_gl_identity(fn)` — Intercept `glLoadIdentity`
- `sdk.on_glu_lookat(fn(...))` — Intercept `gluLookAt`, return `true` to consume

### Lifecycle Hooks
- `sdk.on_load(fn)` — Plugin loaded
- `sdk.on_unload(fn)` — Plugin unloading

## UI Framework API

### Layout
- `TOOLS_UI.header(text)` — Section header
- `TOOLS_UI.subheader(text)` — Subsection header
- `TOOLS_UI.status_badge(active, on?, off?)` — Status indicator
- `TOOLS_UI.keybind(key, desc)` — Keybind display
- `TOOLS_UI.group_begin(label)` / `TOOLS_UI.group_end()` — Styled group

### Widgets
- `TOOLS_UI.checkbox(label, tbl, key, tip?)` — Boolean toggle
- `TOOLS_UI.slider_float(label, tbl, key, min, max, tip?)` — Float slider
- `TOOLS_UI.drag_float(label, tbl, key, speed, min, max, tip?)` — Float drag
- `TOOLS_UI.slider_int(label, tbl, key, min, max, tip?)` — Integer slider
- `TOOLS_UI.color_edit4(label_rgb, label_alpha, color_tbl)` — Color picker
- `TOOLS_UI.action_button(label, tip?)` — Clickable button

### Panel Management
- `TOOLS_UI.register_panel(id, title, draw_fn)` — Register panel
- `TOOLS_UI.unregister_panel(id)` — Unregister panel
- `TOOLS_UI.safe_call(fn, ...)` — Safe function execution

## SDK Functions

### Logging
- `sdk.log_info(msg)`
- `sdk.log_warn(msg)`
- `sdk.log_error(msg)`

### Input
- `sdk.is_key_down(vk)` → `boolean`
- `sdk.get_cursor_pos()` → `x, y`
- `sdk.set_cursor_pos(x, y)`
- `sdk.show_cursor(show)`
- `sdk.send_chars(text)`

### Window
- `sdk.get_window_rect()` → `left, top, right, bottom`

### OpenGL
- `sdk.gl_enable(cap)` / `sdk.gl_disable(cap)`
- `sdk.gl_depth_mask(flag)`
- `sdk.gl_blend_func(sfactor, dfactor)`
- `sdk.gl_line_width(width)`
- `sdk.gl_color4f(r, g, b, a)`
- `sdk.gl_polygon_mode(face, mode)`
- `sdk.gl_push_attrib(mask)` / `sdk.gl_pop_attrib()`
- `sdk.gl_push_matrix()` / `sdk.gl_pop_matrix()`
- `sdk.gl_mult_matrix_d(matrix)`
- `sdk.gl_apply_lookat(...)`
- `sdk.gl_begin(mode)` / `sdk.gl_end()`
- `sdk.gl_vertex3f(x, y, z)`

### Math
- `gmath.radians(deg)` → `rad`
- `gmath.degrees(rad)` → `deg`
- `gmath.sin(x)`, `gmath.cos(x)`, `gmath.tan(x)`
- `gmath.normalize(x, y, z)` → `nx, ny, nz`
- `gmath.cross(ax, ay, az, bx, by, bz)` → `cx, cy, cz`
- `gmath.clamp(value, min, max)` → `clamped`
- `gmath.mod(value, mod)` → `result`

### Constants
- `VK.*` — Virtual key codes (e.g., `VK.F1`, `VK.SPACE`)
- `GL.*` — OpenGL constants (e.g., `GL.DEPTH_TEST`, `GL.BLEND`)

## Best Practices

### Error Handling
```lua
local ok, err = TOOLS_UI.safe_call(function()
    -- Risky operation
end)
if not ok then
    sdk.log_error("Operation failed: " .. tostring(err))
end
```

### Resource Management
```lua
sdk.on_unload(function()
    -- Cleanup resources
    if state.mouse_look then
        sdk.show_cursor(true)
    end
end)
```

### Performance
```lua
-- Cache frequently accessed values
local is_key_down = sdk.is_key_down
local VK_SHIFT = VK.SHIFT

sdk.on_frame(function()
    if is_key_down(VK_SHIFT) then
        -- Fast check
    end
end)
```

### Validation
```lua
if type(cfg.speed) ~= "number" then
    sdk.log_error("Invalid speed configuration")
    return
end
```

## Lua 5.5.0 Features

This plugin system leverages modern Lua features:
- **Type annotations** via LuaLS
- **Error handling** with `xpcall` and `debug.traceback`
- **Performance** with local variable caching
- **Safety** with input validation
- **Modularity** with proper module pattern

## Examples

See included plugins:
- `cheats.lua` — Cheat code system
- `freecam.lua` — Free camera with WASD + mouse look
- `wallhack.lua` — Visual overlay modes
- `world_grid.lua` — 3D reference grid

## Development

### Testing
1. Place shared plugins in `lua/` (deployed to each game's runtime `plugins/`)
2. Launch game
3. Press `[INSERT]` to open UI
4. Check logs for errors

### Debugging
```lua
sdk.log_info(string.format("Debug: %s", tostring(value)))
```

### Hotkeys
- `[INSERT]` — Toggle UI
- Plugin-specific hotkeys defined in each plugin

## License

MIT License — See project root for details.
