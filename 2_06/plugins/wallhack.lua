---@meta
--- Airstrike 3D Tools — Wallhack / Visual-Mod Plugin
--- Professional visual overlay system with multiple rendering modes.
---
--- @module wallhack
--- @author Airstrike 3D Tools Team
--- @license MIT
--- @version 3.0.0
---
--- Four overlay modes for seeing through geometry:
---   1. X-Ray (depth disable)
---   2. Wireframe overlay
---   3. Ghost (transparent + wireframe)
---   4. Z-Bias only
---
--- Hotkeys:
---   F2 — Toggle wallhack on/off
---   F3 — Cycle through modes (while enabled)

local M = {}

-- ── Configuration ───────────────────────────────────────────────────────────

---@class WallhackConfig
---@field enabled boolean
---@field mode integer Current mode (1-4)
---@field wire_color number[] RGBA color
---@field wire_width number Line width
---@field bias_amount number Z-bias offset
---@field xray_alpha number Ghost transparency

---@type WallhackConfig
local cfg = {
    enabled = true,
    mode = 1,
    wire_color = { 0.0, 1.0, 0.0, 0.7 },
    wire_width = 1.5,
    bias_amount = -0.05,
    xray_alpha = 0.3,
}

---@type WallhackConfig
local DEFAULT_CFG = {
    enabled = true,
    mode = 1,
    wire_color = { 0.0, 1.0, 0.0, 0.7 },
    wire_width = 1.5,
    bias_amount = -0.05,
    xray_alpha = 0.3,
}

-- ── Mode Definitions ────────────────────────────────────────────────────────

---@class ModeDefinition
---@field name string Display name
---@field desc string Description
---@field needs_restore boolean Whether mode needs state restoration

---@type ModeDefinition[]
local MODES = {
    {
        name = "X-Ray",
        desc = "Depth disable for see-through rendering",
        needs_restore = false,
    },
    {
        name = "Wireframe",
        desc = "Polygon wireframe overlay",
        needs_restore = false,
    },
    {
        name = "Ghost",
        desc = "Transparent wireframe overlay",
        needs_restore = false,
    },
    {
        name = "Z-Bias",
        desc = "Depth offset to reveal overlapping geometry",
        needs_restore = true,
    },
}

-- ── Constants ───────────────────────────────────────────────────────────────

local VK_TOGGLE = VK.F2
local VK_CYCLE = VK.F3
local MODE_COUNT = #MODES

-- ── Performance Optimizations ───────────────────────────────────────────────

local format = string.format
local gl_disable = sdk.gl_disable
local gl_enable = sdk.gl_enable
local gl_depth_mask = sdk.gl_depth_mask
local gl_polygon_mode = sdk.gl_polygon_mode
local gl_line_width = sdk.gl_line_width
local gl_color4f = sdk.gl_color4f
local gl_blend_func = sdk.gl_blend_func
local gl_push_matrix = sdk.gl_push_matrix
local gl_pop_matrix = sdk.gl_pop_matrix
local gl_mult_matrix_d = sdk.gl_mult_matrix_d
local gl_push_attrib = sdk.gl_push_attrib
local gl_pop_attrib = sdk.gl_pop_attrib
local log_info = sdk.log_info
local log_warn = sdk.log_warn

-- Cache GL constants
local GL_DEPTH_TEST = GL.DEPTH_TEST
local GL_LIGHTING = GL.LIGHTING
local GL_TEXTURE_2D = GL.TEXTURE_2D
local GL_BLEND = GL.BLEND
local GL_SRC_ALPHA = GL.SRC_ALPHA
local GL_ONE_MINUS_SRC_ALPHA = GL.ONE_MINUS_SRC_ALPHA
local GL_FRONT_AND_BACK = GL.FRONT_AND_BACK
local GL_LINE = GL.LINE
local GL_ALL_ATTRIB_BITS = GL.ALL_ATTRIB_BITS

-- ── Mode Applicators ────────────────────────────────────────────────────────

---Apply X-Ray mode (depth disable)
local function apply_mode_xray()
    gl_disable(GL_DEPTH_TEST)
    gl_depth_mask(false)
end

---Apply Wireframe mode
local function apply_mode_wireframe()
    gl_polygon_mode(GL_FRONT_AND_BACK, GL_LINE)
    gl_line_width(cfg.wire_width)
    gl_disable(GL_LIGHTING)
    gl_disable(GL_TEXTURE_2D)
    
    local wc = cfg.wire_color
    gl_color4f(wc[1], wc[2], wc[3], wc[4])
end

---Apply Ghost mode (transparent + wireframe)
local function apply_mode_ghost()
    gl_disable(GL_DEPTH_TEST)
    gl_depth_mask(false)
    gl_enable(GL_BLEND)
    gl_blend_func(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)
    gl_color4f(1, 1, 1, cfg.xray_alpha)
    gl_polygon_mode(GL_FRONT_AND_BACK, GL_LINE)
    gl_line_width(cfg.wire_width)
end

---Apply Z-Bias mode (depth offset)
local function apply_mode_zbias()
    gl_push_matrix()
    gl_mult_matrix_d({
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, cfg.bias_amount, 1,
    })
end

---@type function[]
local mode_applicators = {
    apply_mode_xray,
    apply_mode_wireframe,
    apply_mode_ghost,
    apply_mode_zbias,
}

-- ── State Management ────────────────────────────────────────────────────────

---@class WallhackState
---@field mode_changed boolean Whether mode changed this frame
---@field last_mode integer Previous mode
---@field activation_count integer Total activations

local state = {
    mode_changed = false,
    last_mode = 1,
    activation_count = 0,
}

---Validate configuration
---@return boolean valid
local function validate_config()
    if type(cfg.wire_color) ~= "table" or #cfg.wire_color ~= 4 then
        log_warn("Invalid wire_color, using default")
        cfg.wire_color = DEFAULT_CFG.wire_color
        return false
    end
    
    for i, v in ipairs(cfg.wire_color) do
        if type(v) ~= "number" or v < 0 or v > 1 then
            log_warn(format("Invalid wire_color[%d], using default", i))
            cfg.wire_color = DEFAULT_CFG.wire_color
            return false
        end
    end
    
    if type(cfg.wire_width) ~= "number" or cfg.wire_width < 0.1 or cfg.wire_width > 10 then
        log_warn("Invalid wire_width, using default")
        cfg.wire_width = DEFAULT_CFG.wire_width
        return false
    end
    
    if type(cfg.xray_alpha) ~= "number" or cfg.xray_alpha < 0.01 or cfg.xray_alpha > 1 then
        log_warn("Invalid xray_alpha, using default")
        cfg.xray_alpha = DEFAULT_CFG.xray_alpha
        return false
    end
    
    return true
end

---Restore GL state after rendering
local function restore_state()
    local ok, err = TOOLS_UI.safe_call(function()
        gl_pop_attrib()
        
        if MODES[cfg.mode] and MODES[cfg.mode].needs_restore then
            gl_pop_matrix()
        end
    end)
    
    if not ok then
        log_warn(format("Failed to restore GL state: %s", tostring(err)))
    end
end

-- ── Mode Cycling ────────────────────────────────────────────────────────────

---Cycle to next mode
local function cycle_mode_next()
    state.last_mode = cfg.mode
    cfg.mode = (cfg.mode % MODE_COUNT) + 1
    state.mode_changed = true
    log_info(format("Wallhack mode: %s", MODES[cfg.mode].name))
end

---Cycle to previous mode
local function cycle_mode_prev()
    state.last_mode = cfg.mode
    cfg.mode = ((cfg.mode - 2 + MODE_COUNT) % MODE_COUNT) + 1
    state.mode_changed = true
    log_info(format("Wallhack mode: %s", MODES[cfg.mode].name))
end

-- ── Hooks ───────────────────────────────────────────────────────────────────

sdk.on_key_down(function(vk)
    if vk == VK_TOGGLE then
        cfg.enabled = not cfg.enabled
        log_info(format("Wallhack: %s", cfg.enabled and "ON" or "OFF"))
        state.activation_count = state.activation_count + 1
        return true
    end
    
    if vk == VK_CYCLE and cfg.enabled then
        cycle_mode_next()
        return true
    end
    
    return false
end)

sdk.on_gl_identity(function()
    if not cfg.enabled then
        return
    end
    
    local ok, err = TOOLS_UI.safe_call(function()
        gl_push_attrib(GL_ALL_ATTRIB_BITS)
        
        local applicator = mode_applicators[cfg.mode]
        if applicator then
            applicator()
        else
            log_warn(format("Invalid mode %d, falling back to mode 1", cfg.mode))
            cfg.mode = 1
            mode_applicators[1]()
        end
    end)
    
    if not ok then
        log_warn(format("Failed to apply wallhack mode: %s", tostring(err)))
    end
end)

sdk.on_frame(function()
    if cfg.enabled then
        restore_state()
    end
end)

-- ── UI Panel ────────────────────────────────────────────────────────────────

local function draw_panel()
    TOOLS_UI.header("Wallhack")
    TOOLS_UI.status_badge(cfg.enabled)
    ui.same_line()
    ui.text("wallhack state")
    
    TOOLS_UI.checkbox(
        "Enable Wallhack", cfg, "enabled",
        "Visual overlay modes for seeing through geometry"
    )
    
    if not cfg.enabled then
        return
    end
    
    -- Mode selector
    TOOLS_UI.header("Mode")
    
    local current_mode = MODES[cfg.mode]
    if current_mode then
        ui.text("Active:")
        ui.same_line()
        ui.text_colored(0.26, 0.59, 0.98, 1.0, current_mode.name)
        ui.tooltip(current_mode.desc)
    end
    
    if ui.button("Previous##wallhack") then
        cycle_mode_prev()
    end
    ui.tooltip("Switch to the previous mode")
    ui.same_line()
    
    if ui.button("Next##wallhack") then
        cycle_mode_next()
    end
    ui.tooltip("Switch to the next mode")
    ui.same_line()
    ui.text_disabled("or press F3")
    ui.spacing()
    
    -- Mode-specific settings
    if cfg.mode == 2 or cfg.mode == 3 then
        TOOLS_UI.slider_float(
            "Line Width", cfg, "wire_width",
            0.5, 5.0,
            "Thickness of wireframe lines"
        )
        TOOLS_UI.color_edit4("Wire Color", "Wire Alpha", cfg.wire_color)
    end
    
    if cfg.mode == 3 then
        TOOLS_UI.slider_float(
            "Ghost Alpha", cfg, "xray_alpha",
            0.05, 0.8,
            "Transparency of the ghost overlay"
        )
    end
    
    if cfg.mode == 4 then
        TOOLS_UI.slider_float(
            "Z Bias", cfg, "bias_amount",
            -1.0, 0.0,
            "Subtle depth offset to reveal overlapping geometry"
        )
    end
    
    -- All modes list
    if ui.collapsing_header("All Modes", false) then
        for i, mode in ipairs(MODES) do
            local is_current = (i == cfg.mode)
            if is_current then
                ui.text_colored(0.26, 0.59, 0.98, 1.0, format("%d. %s", i, mode.name))
            else
                ui.text(format("%d. %s", i, mode.name))
            end
            ui.same_line()
            ui.text_disabled(format("- %s", mode.desc))
        end
    end
    
    -- Hotkey reference
    ui.spacing()
    TOOLS_UI.keybind("F2", "Toggle wallhack on / off")
    TOOLS_UI.keybind("F3", "Cycle mode (while enabled)")
    
    -- Statistics
    ui.spacing()
    ui.separator()
    ui.text_disabled(format("Activations: %d | Mode changes: tracked",
        state.activation_count))
end

-- ── Registration ────────────────────────────────────────────────────────────

if _G.TOOLS_UI then
    TOOLS_UI.register_panel("wallhack", "Wallhack", draw_panel)
end

-- ── Lifecycle ───────────────────────────────────────────────────────────────

sdk.on_load(function()
    validate_config()
    log_info(format("Wallhack plugin loaded (%d modes)", MODE_COUNT))
end)

sdk.on_unload(function()
    -- Reset state
    state.mode_changed = false
    state.last_mode = 1
    state.activation_count = 0
    
    log_info("Wallhack plugin unloaded")
end)

-- ── Export ──────────────────────────────────────────────────────────────────

return M
