---@meta
--- Airstrike 3D Tools — Plugin Template
--- Professional plugin template with best practices.
---
--- @module plugin_template
--- @author Your Name
--- @license MIT
--- @version 1.0.0
---
--- Description of what this plugin does.

local M = {}

-- ── Configuration ───────────────────────────────────────────────────────────

---@class PluginConfig
---@field enabled boolean Main toggle
---@field example_value number Example config value
---@field example_color number[] Example color (RGBA)

---@type PluginConfig
local cfg = {
    enabled = true,
    example_value = 1.0,
    example_color = { 1.0, 1.0, 1.0, 1.0 },
}

---@type PluginConfig
local DEFAULT_CFG = {
    enabled = true,
    example_value = 1.0,
    example_color = { 1.0, 1.0, 1.0, 1.0 },
}

-- ── State ───────────────────────────────────────────────────────────────────

---@class PluginState
---@field activation_count integer

local state = {
    activation_count = 0,
}

-- ── Performance Optimizations ───────────────────────────────────────────────

-- Cache frequently accessed functions
local format = string.format
local log_info = sdk.log_info
local log_warn = sdk.log_warn
local log_error = sdk.log_error

-- Cache virtual key codes
local VK_EXAMPLE = VK.F1

-- ── Validation ──────────────────────────────────────────────────────────────

---Validate configuration
---@return boolean valid
local function validate_config()
    if type(cfg.example_value) ~= "number" or cfg.example_value < 0 then
        log_warn("Invalid example_value, using default")
        cfg.example_value = DEFAULT_CFG.example_value
        return false
    end
    
    if type(cfg.example_color) ~= "table" or #cfg.example_color ~= 4 then
        log_warn("Invalid example_color, using default")
        cfg.example_color = DEFAULT_CFG.example_color
        return false
    end
    
    return true
end

-- ── Core Logic ──────────────────────────────────────────────────────────────

---Example function with error handling
local function example_function()
    local ok, err = TOOLS_UI.safe_call(function()
        -- Your logic here
        state.activation_count = state.activation_count + 1
    end)
    
    if not ok then
        log_error(format("Example function failed: %s", tostring(err)))
        return false
    end
    
    return true
end

-- ── Hooks ───────────────────────────────────────────────────────────────────

-- Frame update (called every frame)
sdk.on_frame(function()
    if not cfg.enabled then
        return
    end
    
    -- Your per-frame logic here
end)

-- Key down handler
sdk.on_key_down(function(vk)
    if vk == VK_EXAMPLE then
        example_function()
        log_info(format("Example activated (count: %d)", state.activation_count))
        return true -- consumed
    end
    return false -- not consumed
end)

-- Renderer-neutral graphics are available through sdk.camera_*,
-- sdk.set_world_lines(), and sdk.set_visual_mode(). Check a feature before use:
local has_world_lines = sdk.has_graphics_capability("world_lines")

-- ── UI Panel ────────────────────────────────────────────────────────────────

local function draw_panel()
    TOOLS_UI.header("Plugin Template")
    TOOLS_UI.status_badge(cfg.enabled)
    ui.same_line()
    ui.text("plugin state")
    
    TOOLS_UI.checkbox(
        "Enable Plugin", cfg, "enabled",
        "Main toggle for this plugin"
    )
    
    if not cfg.enabled then
        return
    end
    
    -- Settings section
    if ui.collapsing_header("Settings", true) then
        TOOLS_UI.slider_float(
            "Example Value", cfg, "example_value",
            0.0, 10.0,
            "Example configuration value"
        )
        
        TOOLS_UI.color_edit4(
            "Example Color", "Example Alpha",
            cfg.example_color
        )
    end
    
    -- Actions section
    if ui.collapsing_header("Actions", false) then
        if TOOLS_UI.action_button("Example Action", "Perform example action") then
            example_function()
        end
        
        ui.spacing()
        ui.text_disabled(format("Activations: %d", state.activation_count))
    end
    
    -- Hotkey reference
    ui.spacing()
    TOOLS_UI.keybind("F1", "Example hotkey")
end

-- ── Registration ────────────────────────────────────────────────────────────

if _G.TOOLS_UI then
    TOOLS_UI.register_panel("plugin_template", "Plugin Template", draw_panel)
end

-- ── Lifecycle ───────────────────────────────────────────────────────────────

sdk.on_load(function()
    validate_config()
    log_info("Plugin template loaded")
end)

sdk.on_unload(function()
    -- Cleanup state
    state.activation_count = 0
    
    log_info("Plugin template unloaded")
end)

-- ── Export ──────────────────────────────────────────────────────────────────

return M
