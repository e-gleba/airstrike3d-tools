# Lua Plugin Examples

Practical examples demonstrating the plugin API and best practices.

## Table of Contents

1. [Basic Plugin](#basic-plugin)
2. [Stateful Plugin](#stateful-plugin)
3. [GL Rendering Plugin](#gl-rendering-plugin)
4. [Input Handling Plugin](#input-handling-plugin)
5. [Configuration Plugin](#configuration-plugin)
6. [Error Handling](#error-handling)
7. [Performance Optimization](#performance-optimization)

## Basic Plugin

Minimal plugin that logs a message on load.

```lua
---@meta
--- Basic logging plugin
--- @module basic_example
--- @version 1.0.0

local M = {}

sdk.on_load(function()
    sdk.log_info("Basic plugin loaded!")
end)

sdk.on_unload(function()
    sdk.log_info("Basic plugin unloaded!")
end)

return M
```

## Stateful Plugin

Plugin with persistent state and statistics.

```lua
---@meta
--- Counter plugin with state management
--- @module counter_example
--- @version 1.0.0

local M = {}

-- State
local state = {
    count = 0,
    last_increment = 0,
}

-- Increment counter
local function increment()
    state.count = state.count + 1
    state.last_increment = os.clock()
    sdk.log_info(string.format("Count: %d", state.count))
end

-- UI Panel
local function draw_panel()
    TOOLS_UI.header("Counter")
    
    ui.text(string.format("Count: %d", state.count))
    
    if state.last_increment > 0 then
        local elapsed = os.clock() - state.last_increment
        ui.text_disabled(string.format("Last increment: %.1fs ago", elapsed))
    end
    
    ui.spacing()
    if TOOLS_UI.action_button("Increment", "Add 1 to counter") then
        increment()
    end
    
    ui.same_line()
    if TOOLS_UI.action_button("Reset", "Reset counter to 0") then
        state.count = 0
        sdk.log_info("Counter reset")
    end
end

-- Registration
if _G.TOOLS_UI then
    TOOLS_UI.register_panel("counter", "Counter", draw_panel)
end

sdk.on_load(function()
    sdk.log_info("Counter plugin loaded")
end)

sdk.on_unload(function()
    state.count = 0
    state.last_increment = 0
    sdk.log_info("Counter plugin unloaded")
end)

return M
```

## GL Rendering Plugin

Plugin that renders custom OpenGL geometry.

```lua
---@meta
--- Triangle rendering plugin
--- @module triangle_example
--- @version 1.0.0

local M = {}

-- Configuration
local cfg = {
    enabled = true,
    size = 10.0,
    color = { 1.0, 0.5, 0.0, 1.0 },
}

-- Cache GL functions
local gl_push_attrib = sdk.gl_push_attrib
local gl_pop_attrib = sdk.gl_pop_attrib
local gl_disable = sdk.gl_disable
local gl_enable = sdk.gl_enable
local gl_color4f = sdk.gl_color4f
local gl_begin = sdk.gl_begin
local gl_end = sdk.gl_end
local gl_vertex3f = sdk.gl_vertex3f

-- Draw triangle
local function draw_triangle()
    local ok, err = TOOLS_UI.safe_call(function()
        local c = cfg.color
        local s = cfg.size
        
        gl_color4f(c[1], c[2], c[3], c[4])
        gl_begin(GL.TRIANGLES)
        
        gl_vertex3f(0, s, 0)      -- Top
        gl_vertex3f(-s, -s, 0)    -- Bottom left
        gl_vertex3f(s, -s, 0)     -- Bottom right
        
        gl_end()
    end)
    
    if not ok then
        sdk.log_error("Failed to draw triangle: " .. tostring(err))
    end
end

-- GL hook
sdk.on_gl_identity(function()
    if not cfg.enabled then
        return
    end
    
    local ok, err = TOOLS_UI.safe_call(function()
        gl_push_attrib(GL.ALL_ATTRIB_BITS)
        gl_disable(GL.DEPTH_TEST)
        gl_disable(GL.TEXTURE_2D)
        gl_enable(GL.BLEND)
        
        draw_triangle()
        
        gl_pop_attrib()
    end)
    
    if not ok then
        sdk.log_error("GL hook failed: " .. tostring(err))
    end
end)

-- UI Panel
local function draw_panel()
    TOOLS_UI.header("Triangle")
    TOOLS_UI.status_badge(cfg.enabled)
    
    TOOLS_UI.checkbox("Enable", cfg, "enabled", "Show triangle")
    
    if cfg.enabled then
        TOOLS_UI.slider_float("Size", cfg, "size", 1, 100, "Triangle size")
        TOOLS_UI.color_edit4("Color", "Alpha", cfg.color)
    end
end

if _G.TOOLS_UI then
    TOOLS_UI.register_panel("triangle", "Triangle", draw_panel)
end

sdk.on_load(function()
    sdk.log_info("Triangle plugin loaded")
end)

return M
```

## Input Handling Plugin

Plugin demonstrating keyboard and mouse input.

```lua
---@meta
--- Input monitoring plugin
--- @module input_example
--- @version 1.0.0

local M = {}

-- State
local state = {
    last_key = "None",
    last_mouse = { x = 0, y = 0 },
    key_count = 0,
}

-- Cache functions
local is_key_down = sdk.is_key_down
local get_cursor_pos = sdk.get_cursor_pos

-- Key handler
sdk.on_key_down(function(vk)
    state.last_key = string.format("VK_%d", vk)
    state.key_count = state.key_count + 1
    
    sdk.log_info(string.format("Key pressed: %s (count: %d)",
        state.last_key, state.key_count))
    
    return false -- Don't consume
end)

-- Frame update
sdk.on_frame(function()
    state.last_mouse.x, state.last_mouse.y = get_cursor_pos()
end)

-- UI Panel
local function draw_panel()
    TOOLS_UI.header("Input Monitor")
    
    ui.text(string.format("Last Key: %s", state.last_key))
    ui.text(string.format("Mouse: %d, %d",
        state.last_mouse.x, state.last_mouse.y))
    ui.text(string.format("Key Presses: %d", state.key_count))
    
    ui.spacing()
    ui.separator()
    
    -- Live key states
    TOOLS_UI.header("Key States")
    
    local keys = {
        { name = "Shift", vk = VK.SHIFT },
        { name = "Control", vk = VK.CONTROL },
        { name = "Space", vk = VK.SPACE },
        { name = "Left Mouse", vk = VK.LBUTTON },
        { name = "Right Mouse", vk = VK.RBUTTON },
    }
    
    for _, key in ipairs(keys) do
        local down = is_key_down(key.vk)
        TOOLS_UI.status_badge(down, key.name, key.name)
        ui.same_line()
        ui.text_disabled(string.format("(VK_%d)", key.vk))
    end
    
    ui.spacing()
    if TOOLS_UI.action_button("Reset Stats", "Clear statistics") then
        state.key_count = 0
        state.last_key = "None"
        sdk.log_info("Input stats reset")
    end
end

if _G.TOOLS_UI then
    TOOLS_UI.register_panel("input", "Input Monitor", draw_panel)
end

sdk.on_load(function()
    sdk.log_info("Input monitor loaded")
end)

sdk.on_unload(function()
    state.key_count = 0
    sdk.log_info("Input monitor unloaded")
end)

return M
```

## Configuration Plugin

Plugin with comprehensive configuration management.

```lua
---@meta
--- Configuration example plugin
--- @module config_example
--- @version 1.0.0

local M = {}

-- Configuration with validation
---@class ConfigExample
---@field enabled boolean
---@field speed number
---@field color number[]
---@field mode integer
---@field label string

---@type ConfigExample
local cfg = {
    enabled = true,
    speed = 1.0,
    color = { 0.5, 0.8, 1.0, 1.0 },
    mode = 1,
    label = "Default",
}

-- Defaults for validation
local DEFAULTS = {
    speed = 1.0,
    color = { 0.5, 0.8, 1.0, 1.0 },
    mode = 1,
    label = "Default",
}

-- Validation
local function validate_config()
    local valid = true
    
    if type(cfg.speed) ~= "number" or cfg.speed < 0.1 or cfg.speed > 10 then
        sdk.log_warn("Invalid speed, using default")
        cfg.speed = DEFAULTS.speed
        valid = false
    end
    
    if type(cfg.color) ~= "table" or #cfg.color ~= 4 then
        sdk.log_warn("Invalid color, using default")
        cfg.color = DEFAULTS.color
        valid = false
    end
    
    if type(cfg.mode) ~= "number" or cfg.mode < 1 or cfg.mode > 3 then
        sdk.log_warn("Invalid mode, using default")
        cfg.mode = DEFAULTS.mode
        valid = false
    end
    
    if type(cfg.label) ~= "string" or #cfg.label == 0 then
        sdk.log_warn("Invalid label, using default")
        cfg.label = DEFAULTS.label
        valid = false
    end
    
    return valid
end

-- Mode names
local MODE_NAMES = { "Mode A", "Mode B", "Mode C" }

-- UI Panel
local function draw_panel()
    TOOLS_UI.header("Configuration Example")
    TOOLS_UI.status_badge(cfg.enabled)
    
    TOOLS_UI.checkbox("Enable", cfg, "enabled", "Main toggle")
    
    if not cfg.enabled then
        return
    end
    
    -- Numeric settings
    if ui.collapsing_header("Numeric Settings", true) then
        TOOLS_UI.slider_float("Speed", cfg, "speed", 0.1, 10.0, "Movement speed")
        TOOLS_UI.slider_int("Mode", cfg, "mode", 1, 3, "Operation mode")
        
        ui.text(string.format("Current mode: %s", MODE_NAMES[cfg.mode]))
    end
    
    -- Color settings
    if ui.collapsing_header("Color Settings", true) then
        TOOLS_UI.color_edit4("Main Color", "Alpha", cfg.color)
        
        ui.spacing()
        ui.text("Preview:")
        ui.same_line()
        ui.text_colored(cfg.color[1], cfg.color[2], cfg.color[3], cfg.color[4],
            "████████")
    end
    
    -- Text settings
    if ui.collapsing_header("Text Settings", false) then
        ui.text("Label:")
        ui.same_line()
        ui.text_colored(0.26, 0.59, 0.98, 1.0, cfg.label)
    end
    
    -- Actions
    ui.spacing()
    if TOOLS_UI.action_button("Reset to Defaults", "Restore all settings") then
        cfg.speed = DEFAULTS.speed
        cfg.color = DEFAULTS.color
        cfg.mode = DEFAULTS.mode
        cfg.label = DEFAULTS.label
        sdk.log_info("Configuration reset to defaults")
    end
end

if _G.TOOLS_UI then
    TOOLS_UI.register_panel("config", "Config Example", draw_panel)
end

sdk.on_load(function()
    validate_config()
    sdk.log_info("Configuration example loaded")
end)

return M
```

## Error Handling

Demonstrates robust error handling patterns.

```lua
---@meta
--- Error handling example
--- @module error_example
--- @version 1.0.0

local M = {}

-- Safe operation with error handling
local function risky_operation(value)
    local ok, result = TOOLS_UI.safe_call(function()
        if type(value) ~= "number" then
            error("Value must be a number")
        end
        
        if value < 0 then
            error("Value must be non-negative")
        end
        
        -- Simulate work
        return value * 2
    end)
    
    if not ok then
        sdk.log_error(string.format("Operation failed: %s", tostring(result)))
        return nil
    end
    
    sdk.log_info(string.format("Operation succeeded: %d", result))
    return result
end

-- Graceful degradation
local function operation_with_fallback(value, fallback)
    local result = risky_operation(value)
    
    if result == nil then
        sdk.log_warn("Using fallback value")
        return fallback
    end
    
    return result
end

-- UI Panel
local function draw_panel()
    TOOLS_UI.header("Error Handling")
    
    ui.text_wrapped("This plugin demonstrates error handling patterns.")
    ui.spacing()
    
    if TOOLS_UI.action_button("Valid Operation", "Call with valid input") then
        risky_operation(42)
    end
    
    if TOOLS_UI.action_button("Invalid Type", "Call with wrong type") then
        risky_operation("not a number")
    end
    
    if TOOLS_UI.action_button("Invalid Value", "Call with negative number") then
        risky_operation(-5)
    end
    
    ui.spacing()
    ui.separator()
    
    if TOOLS_UI.action_button("With Fallback", "Use fallback on error") then
        local result = operation_with_fallback("invalid", 100)
        sdk.log_info(string.format("Result: %d", result))
    end
end

if _G.TOOLS_UI then
    TOOLS_UI.register_panel("error", "Error Handling", draw_panel)
end

sdk.on_load(function()
    sdk.log_info("Error handling example loaded")
end)

return M
```

## Performance Optimization

Demonstrates performance best practices.

```lua
---@meta
--- Performance optimization example
--- @module perf_example
--- @version 1.0.0

local M = {}

-- State
local state = {
    frame_count = 0,
    last_fps = 0,
    operations = 0,
}

-- ✅ GOOD: Cache frequently accessed functions
local log_info = sdk.log_info
local is_key_down = sdk.is_key_down
local get_delta_time = ui.get_delta_time
local get_framerate = ui.get_framerate

-- ✅ GOOD: Cache constants
local VK_SHIFT = VK.SHIFT
local VK_CONTROL = VK.CONTROL

-- ✅ GOOD: Use local variables in loops
local function optimized_loop(iterations)
    local count = 0
    for i = 1, iterations do
        count = count + i
    end
    state.operations = state.operations + count
end

-- ✅ GOOD: Avoid table creation in hot paths
local reusable_table = { 0, 0, 0 }

local function update_reusable(x, y, z)
    reusable_table[1] = x
    reusable_table[2] = y
    reusable_table[3] = z
end

-- Frame update (hot path)
sdk.on_frame(function()
    state.frame_count = state.frame_count + 1
    
    -- Update FPS every 30 frames
    if state.frame_count % 30 == 0 then
        state.last_fps = get_framerate()
    end
    
    -- ✅ GOOD: Early exit for disabled features
    if not is_key_down(VK_SHIFT) then
        return
    end
    
    -- Do expensive work only when needed
    local dt = get_delta_time()
    optimized_loop(1000)
end)

-- UI Panel
local function draw_panel()
    TOOLS_UI.header("Performance")
    
    ui.text(string.format("FPS: %.1f", state.last_fps))
    ui.text(string.format("Frames: %d", state.frame_count))
    ui.text(string.format("Operations: %d", state.operations))
    
    ui.spacing()
    ui.separator()
    
    TOOLS_UI.header("Best Practices")
    
    ui.text_wrapped("✅ Cache frequently accessed functions")
    ui.text_wrapped("✅ Cache virtual key constants")
    ui.text_wrapped("✅ Use local variables in loops")
    ui.text_wrapped("✅ Avoid table creation in hot paths")
    ui.text_wrapped("✅ Early exit for disabled features")
    ui.text_wrapped("✅ Reuse tables when possible")
    
    ui.spacing()
    if TOOLS_UI.action_button("Reset Stats", "Clear all counters") then
        state.frame_count = 0
        state.operations = 0
        log_info("Performance stats reset")
    end
end

if _G.TOOLS_UI then
    TOOLS_UI.register_panel("perf", "Performance", draw_panel)
end

sdk.on_load(function()
    log_info("Performance example loaded")
end)

return M
```

## Summary

These examples demonstrate:

1. **Basic structure**: Module pattern, lifecycle hooks
2. **State management**: Persistent state, statistics
3. **GL rendering**: Safe OpenGL operations
4. **Input handling**: Keyboard and mouse input
5. **Configuration**: Validation and defaults
6. **Error handling**: Safe calls, graceful degradation
7. **Performance**: Caching, optimization patterns

Use these as starting points for your own plugins!
