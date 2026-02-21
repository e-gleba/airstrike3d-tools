-- Wallhack / visual-mod plugin: X-Ray, wireframe, ghost, Z-bias modes

---@class WireColor
---@field [1] number  red
---@field [2] number  green
---@field [3] number  blue
---@field [4] number  alpha

---@class WallhackConfig
---@field enabled      boolean
---@field mode         integer
---@field depth_disable boolean
---@field wireframe    boolean
---@field wire_color   WireColor
---@field wire_width   number
---@field z_bias       boolean
---@field bias_amount  number
---@field xray_alpha   number

---@type WallhackConfig
local cfg = {
    enabled = true,
    mode = 1,
    depth_disable = true,
    wireframe = false,
    wire_color = { 0.0, 1.0, 0.0, 0.7 },
    wire_width = 1.5,
    z_bias = true,
    bias_amount = -0.05,
    xray_alpha = 0.3,
}

--- Human-readable names for each wallhack mode, indexed 1–4.
---@type string[]
local MODE_NAMES = {
    "X-Ray (depth disable)",
    "Wireframe overlay",
    "Ghost (transparent + wireframe)",
    "Z-Bias only",
}

--- Hotkeys
local VK_TOGGLE = VK.F2
local VK_CYCLE = VK.F3

-- -----------------------------------------------------------------------
-- Mode applicators
-- -----------------------------------------------------------------------
-- Each mode is a function that sets up the required GL state.
-- Table-driven so adding a new mode is: append a name + an applicator.

---@type (fun())[]
local mode_applicators = {}

-- Mode 1: X-Ray — disable depth test so everything draws on top
mode_applicators[1] = function()
    sdk.gl_disable(GL.DEPTH_TEST)
    sdk.gl_depth_mask(false)
end

-- Mode 2: Wireframe — polygon mode to lines
mode_applicators[2] = function()
    sdk.gl_polygon_mode(GL.FRONT_AND_BACK, GL.LINE)
    sdk.gl_line_width(cfg.wire_width)
    sdk.gl_disable(GL.LIGHTING)
    sdk.gl_disable(GL.TEXTURE_2D)
    local wc = cfg.wire_color
    sdk.gl_color4f(wc[1], wc[2], wc[3], wc[4])
end

-- Mode 3: Ghost — transparent + wireframe
mode_applicators[3] = function()
    sdk.gl_disable(GL.DEPTH_TEST)
    sdk.gl_depth_mask(false)
    sdk.gl_enable(GL.BLEND)
    sdk.gl_blend_func(GL.SRC_ALPHA, GL.ONE_MINUS_SRC_ALPHA)
    sdk.gl_color4f(1, 1, 1, cfg.xray_alpha)
    sdk.gl_polygon_mode(GL.FRONT_AND_BACK, GL.LINE)
    sdk.gl_line_width(cfg.wire_width)
end

-- Mode 4: Z-Bias — subtle depth bias to see overlapping geometry
mode_applicators[4] = function()
    sdk.gl_push_matrix()
    sdk.gl_mult_matrix_d({
        1,
        0,
        0,
        0,
        0,
        1,
        0,
        0,
        0,
        0,
        1,
        0,
        0,
        0,
        cfg.bias_amount,
        1,
    })
end

-- -----------------------------------------------------------------------
-- GL state teardown (per-frame)
-- -----------------------------------------------------------------------

--- Restore GL state pushed during on_gl_identity.
local function restore_state()
    sdk.gl_pop_attrib()
    if cfg.mode == 4 then
        sdk.gl_pop_matrix()
    end
end

-- -----------------------------------------------------------------------
-- Hotkeys
-- -----------------------------------------------------------------------

sdk.on_key_down(function(vk)
    if vk == VK_TOGGLE then
        cfg.enabled = not cfg.enabled
        sdk.log_info(
            string.format("wallhack: %s", cfg.enabled and "ON" or "OFF")
        )
        return true
    end

    if vk == VK_CYCLE and cfg.enabled then
        cfg.mode = (cfg.mode % #MODE_NAMES) + 1
        sdk.log_info(string.format("wallhack mode: %s", MODE_NAMES[cfg.mode]))
        return true
    end

    return false
end)

-- -----------------------------------------------------------------------
-- GL hooks
-- -----------------------------------------------------------------------

sdk.on_gl_identity(function()
    if not cfg.enabled then
        return
    end

    sdk.gl_push_attrib(GL.ALL_ATTRIB_BITS)

    local apply = mode_applicators[cfg.mode]
    if apply then
        apply()
    end
end)

sdk.on_frame(function()
    if not cfg.enabled then
        return
    end
    restore_state()
end)

-- -----------------------------------------------------------------------
-- Overlay UI helpers
-- -----------------------------------------------------------------------

--- Bind a slider_float widget directly to a `cfg` field.
---@param label string
---@param field string
---@param v_min number
---@param v_max number
local function cfg_slider_float(label, field, v_min, v_max)
    cfg[field] = ui.slider_float(label, cfg[field], v_min, v_max)
end

--- Returns true when the current mode uses wireframe parameters.
---@return boolean
local function mode_has_wireframe()
    return cfg.mode == 2 or cfg.mode == 3
end

-- -----------------------------------------------------------------------
-- Overlay UI
-- -----------------------------------------------------------------------

sdk.on_overlay(function()
    if not cfg.enabled then
        return
    end

    ui.set_next_window_pos(10, 500)
    ui.set_next_window_size(250, 0)

    if not ui.begin_window("Wallhack") then
        ui.end_window()
        return
    end

    ui.text(string.format("Mode: %s", MODE_NAMES[cfg.mode]))
    ui.text_disabled("F2 = Toggle | F3 = Cycle")
    ui.separator()

    -- Wireframe controls (modes 2 & 3)
    if mode_has_wireframe() then
        cfg_slider_float("Line Width", "wire_width", 0.5, 5.0)

        local r, g, b, changed = ui.color_edit3(
            "Wire Color",
            cfg.wire_color[1],
            cfg.wire_color[2],
            cfg.wire_color[3]
        )
        if changed then
            cfg.wire_color[1] = r
            cfg.wire_color[2] = g
            cfg.wire_color[3] = b
        end

        cfg.wire_color[4] =
            ui.slider_float("Wire Alpha", cfg.wire_color[4], 0.1, 1.0)
    end

    -- Ghost-specific controls (mode 3)
    if cfg.mode == 3 then
        cfg_slider_float("Ghost Alpha", "xray_alpha", 0.05, 0.8)
    end

    -- Z-Bias controls (mode 4)
    if cfg.mode == 4 then
        cfg_slider_float("Z Bias", "bias_amount", -1.0, 0.0)
    end

    ui.end_window()
end)

-- -----------------------------------------------------------------------
-- Lifecycle
-- -----------------------------------------------------------------------

sdk.on_load(function()
    sdk.log_info(
        string.format("wallhack plugin loaded (%d modes)", #MODE_NAMES)
    )
end)