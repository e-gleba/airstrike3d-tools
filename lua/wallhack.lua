---@meta
--- Visual overlay modes via the renderer-neutral visual API.
---
--- @module wallhack
--- @version 3.1.0
---
--- Modes:
---   1. X-Ray
---   2. Wireframe
---   3. Ghost
---   4. Z-Bias
---
--- Hotkeys:
---   F2 — Toggle
---   F3 — Cycle mode

local M = {}

---@class WallhackConfig
---@field enabled boolean
---@field mode integer
---@field wire_color number[]
---@field wire_width number
---@field bias_amount number
---@field xray_alpha number

local DEFAULT <const> = {
    enabled = true,
    mode = 1,
    wire_color = { 0.0, 1.0, 0.0, 0.7 },
    wire_width = 1.5,
    bias_amount = -0.05,
    xray_alpha = 0.3,
}

---@type WallhackConfig
local cfg = {
    enabled = DEFAULT.enabled,
    mode = DEFAULT.mode,
    wire_color = {
        DEFAULT.wire_color[1],
        DEFAULT.wire_color[2],
        DEFAULT.wire_color[3],
        DEFAULT.wire_color[4],
    },
    wire_width = DEFAULT.wire_width,
    bias_amount = DEFAULT.bias_amount,
    xray_alpha = DEFAULT.xray_alpha,
}

local MODES <const> = {
    { name = "X-Ray", desc = "Depth disable for see-through rendering" },
    { name = "Wireframe", desc = "Polygon wireframe overlay" },
    { name = "Ghost", desc = "Transparent wireframe overlay" },
    { name = "Z-Bias", desc = "Depth offset to reveal overlapping geometry" },
}

local MODE_COUNT <const> = #MODES
local VK_TOGGLE, VK_CYCLE = VK.F2, VK.F3
local format = string.format
local floor = math.floor
local log_info = sdk.log_info
local log_warn = sdk.log_warn

local state = {
    activation_count = 0,
}

local function copy_color(src)
    return { src[1], src[2], src[3], src[4] }
end

local function validate_config()
    if type(cfg.wire_color) ~= "table" or #cfg.wire_color ~= 4 then
        log_warn("Invalid wire_color, using default")
        cfg.wire_color = copy_color(DEFAULT.wire_color)
        return false
    end
    for i, v in ipairs(cfg.wire_color) do
        if type(v) ~= "number" or v < 0 or v > 1 then
            log_warn(format("Invalid wire_color[%d], using default", i))
            cfg.wire_color = copy_color(DEFAULT.wire_color)
            return false
        end
    end
    if type(cfg.wire_width) ~= "number" or cfg.wire_width < 0.1 or cfg.wire_width > 10 then
        log_warn("Invalid wire_width, using default")
        cfg.wire_width = DEFAULT.wire_width
        return false
    end
    if type(cfg.xray_alpha) ~= "number" or cfg.xray_alpha < 0.01 or cfg.xray_alpha > 1 then
        log_warn("Invalid xray_alpha, using default")
        cfg.xray_alpha = DEFAULT.xray_alpha
        return false
    end
    if type(cfg.mode) ~= "number" or cfg.mode < 1 or cfg.mode > MODE_COUNT then
        cfg.mode = DEFAULT.mode
        return false
    end
    return true
end

local function wire_argb()
    local c = cfg.wire_color
    return floor(c[4] * 255) * 0x1000000
        + floor(c[1] * 255) * 0x10000
        + floor(c[2] * 255) * 0x100
        + floor(c[3] * 255)
end

local function publish_mode()
    if not cfg.enabled then
        sdk.set_visual_mode(0, 1.0, 0.0, 0xFFFFFFFF)
        return
    end
    sdk.set_visual_mode(cfg.mode, cfg.xray_alpha, cfg.bias_amount, wire_argb())
end

local function cycle_mode(delta)
    cfg.mode = ((cfg.mode - 1 + delta) % MODE_COUNT) + 1
    log_info(format("Wallhack mode: %s", MODES[cfg.mode].name))
end

sdk.on_key_down(function(vk)
    if vk == VK_TOGGLE then
        cfg.enabled = not cfg.enabled
        log_info(format("Wallhack: %s", cfg.enabled and "ON" or "OFF"))
        state.activation_count = state.activation_count + 1
        return true
    end
    if vk == VK_CYCLE and cfg.enabled then
        cycle_mode(1)
        return true
    end
    return false
end)

sdk.on_frame(publish_mode)

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

    TOOLS_UI.header("Mode")
    local current = MODES[cfg.mode]
    ui.text("Active:")
    ui.same_line()
    ui.text_colored(0.26, 0.59, 0.98, 1.0, current.name)
    ui.tooltip(current.desc)

    if ui.button("Previous##wallhack") then
        cycle_mode(-1)
    end
    ui.tooltip("Switch to the previous mode")
    ui.same_line()
    if ui.button("Next##wallhack") then
        cycle_mode(1)
    end
    ui.tooltip("Switch to the next mode")
    ui.same_line()
    ui.text_disabled("or press F3")
    ui.spacing()

    if cfg.mode == 2 or cfg.mode == 3 then
        TOOLS_UI.slider_float(
            "Line Width", cfg, "wire_width", 0.5, 5.0,
            "Thickness of wireframe lines"
        )
        TOOLS_UI.color_edit4("Wire Color", "Wire Alpha", cfg.wire_color)
    end
    if cfg.mode == 3 then
        TOOLS_UI.slider_float(
            "Ghost Alpha", cfg, "xray_alpha", 0.05, 0.8,
            "Transparency of the ghost overlay"
        )
    end
    if cfg.mode == 4 then
        TOOLS_UI.slider_float(
            "Z Bias", cfg, "bias_amount", -1.0, 0.0,
            "Subtle depth offset to reveal overlapping geometry"
        )
    end

    if ui.collapsing_header("All Modes", false) then
        for i, mode in ipairs(MODES) do
            if i == cfg.mode then
                ui.text_colored(0.26, 0.59, 0.98, 1.0, format("%d. %s", i, mode.name))
            else
                ui.text(format("%d. %s", i, mode.name))
            end
            ui.same_line()
            ui.text_disabled(format("- %s", mode.desc))
        end
    end

    ui.spacing()
    TOOLS_UI.keybind("F2", "Toggle wallhack on / off")
    TOOLS_UI.keybind("F3", "Cycle mode (while enabled)")
    ui.spacing()
    ui.separator()
    ui.text_disabled(format("Activations: %d", state.activation_count))
end

if _G.TOOLS_UI then
    TOOLS_UI.register_panel("wallhack", "Wallhack", draw_panel)
end

sdk.on_load(function()
    validate_config()
    log_info(format("Wallhack plugin loaded (%d modes)", MODE_COUNT))
end)

sdk.on_unload(function()
    sdk.set_visual_mode(0, 1.0, 0.0, 0xFFFFFFFF)
    state.activation_count = 0
    log_info("Wallhack plugin unloaded")
end)

return M
