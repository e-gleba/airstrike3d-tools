-- Wallhack / visual-mod plugin: X-Ray, wireframe, ghost, Z-bias modes

---@class WireColor
---@field [1] number  red
---@field [2] number  green
---@field [3] number  blue
---@field [4] number  alpha

---@class WallhackConfig
---@field enabled       boolean
---@field mode          integer
---@field depth_disable boolean
---@field wireframe     boolean
---@field wire_color    WireColor
---@field wire_width    number
---@field z_bias        boolean
---@field bias_amount   number
---@field xray_alpha    number

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

--- Human-readable names for each wallhack mode.
---@type string[]
local MODE_NAMES = {
    "X-Ray (depth disable)",
    "Wireframe overlay",
    "Ghost (transparent + wireframe)",
    "Z-Bias only",
}

--- Hotkeys
local VK_TOGGLE = VK.F2
local VK_CYCLE  = VK.F3

-- -----------------------------------------------------------------------
-- Mode applicators
-- -----------------------------------------------------------------------

---@type (fun())[]
local mode_applicators = {}

mode_applicators[1] = function()
    sdk.gl_disable(GL.DEPTH_TEST)
    sdk.gl_depth_mask(false)
end

mode_applicators[2] = function()
    sdk.gl_polygon_mode(GL.FRONT_AND_BACK, GL.LINE)
    sdk.gl_line_width(cfg.wire_width)
    sdk.gl_disable(GL.LIGHTING)
    sdk.gl_disable(GL.TEXTURE_2D)
    local wc = cfg.wire_color
    sdk.gl_color4f(wc[1], wc[2], wc[3], wc[4])
end

mode_applicators[3] = function()
    sdk.gl_disable(GL.DEPTH_TEST)
    sdk.gl_depth_mask(false)
    sdk.gl_enable(GL.BLEND)
    sdk.gl_blend_func(GL.SRC_ALPHA, GL.ONE_MINUS_SRC_ALPHA)
    sdk.gl_color4f(1, 1, 1, cfg.xray_alpha)
    sdk.gl_polygon_mode(GL.FRONT_AND_BACK, GL.LINE)
    sdk.gl_line_width(cfg.wire_width)
end

mode_applicators[4] = function()
    sdk.gl_push_matrix()
    sdk.gl_mult_matrix_d({
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, cfg.bias_amount, 1,
    })
end

-- -----------------------------------------------------------------------
-- GL state teardown
-- -----------------------------------------------------------------------

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
        sdk.log_info(string.format("wallhack: %s", cfg.enabled and "ON" or "OFF"))
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
-- UI Panel
-- -----------------------------------------------------------------------

local function draw_panel()
    TOOLS_UI.header("Wallhack")
    TOOLS_UI.status_badge(cfg.enabled)
    ui.same_line()
    ui.text("wallhack state")

    TOOLS_UI.checkbox("Enable Wallhack", cfg, "enabled",
        "Visual overlay modes for seeing through geometry")

    if not cfg.enabled then
        return
    end

    -- Mode selector (button-based; avoids C++ combo binding crash)
    TOOLS_UI.header("Mode")
    ui.text("Active:")
    ui.same_line()
    ui.text_colored(0.26, 0.59, 0.98, 1.0, MODE_NAMES[cfg.mode])
    ui.tooltip("Current wallhack rendering mode")

    if ui.button("Previous##wallhack") then
        cfg.mode = ((cfg.mode - 2 + #MODE_NAMES) % #MODE_NAMES) + 1
    end
    ui.tooltip("Switch to the previous mode")
    ui.same_line()
    if ui.button("Next##wallhack") then
        cfg.mode = (cfg.mode % #MODE_NAMES) + 1
    end
    ui.tooltip("Switch to the next mode")
    ui.same_line()
    ui.text_disabled("or press F3")

    ui.spacing()

    if cfg.mode == 2 or cfg.mode == 3 then
        TOOLS_UI.slider_float("Line Width", cfg, "wire_width", 0.5, 5.0,
            "Thickness of wireframe lines")
        TOOLS_UI.color_edit4("Wire Color", "Wire Alpha", cfg.wire_color)
    end

    if cfg.mode == 3 then
        TOOLS_UI.slider_float("Ghost Alpha", cfg, "xray_alpha", 0.05, 0.8,
            "Transparency of the ghost overlay")
    end

    if cfg.mode == 4 then
        TOOLS_UI.slider_float("Z Bias", cfg, "bias_amount", -1.0, 0.0,
            "Subtle depth offset to reveal overlapping geometry")
    end

    ui.spacing()
    TOOLS_UI.keybind("F2", "Toggle wallhack on / off")
    TOOLS_UI.keybind("F3", "Cycle mode (while enabled)")
end

if _G.TOOLS_UI then
    TOOLS_UI.register_panel("wallhack", "Wallhack", draw_panel)
end

-- -----------------------------------------------------------------------
-- Lifecycle
-- -----------------------------------------------------------------------

sdk.on_load(function()
    sdk.log_info(string.format("wallhack plugin loaded (%d modes)", #MODE_NAMES))
end)
