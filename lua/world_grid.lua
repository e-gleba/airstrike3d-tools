---@meta
--- World grid and axes overlay via the renderer-neutral world-lines API.
---
--- @module world_grid
--- @version 3.1.0
---
--- Hotkey: F7 — toggle

local M = {}

---@class GridConfig
---@field enabled boolean
---@field grid_size integer
---@field grid_step integer
---@field grid_color number[]
---@field grid_y number
---@field axes boolean
---@field axes_len number

local DEFAULT <const> = {
    enabled = true,
    grid_size = 100,
    grid_step = 10,
    grid_color = { 0.3, 0.3, 0.3, 0.4 },
    grid_y = 0,
    axes = true,
    axes_len = 50,
}

---@type GridConfig
local cfg = {
    enabled = DEFAULT.enabled,
    grid_size = DEFAULT.grid_size,
    grid_step = DEFAULT.grid_step,
    grid_color = {
        DEFAULT.grid_color[1],
        DEFAULT.grid_color[2],
        DEFAULT.grid_color[3],
        DEFAULT.grid_color[4],
    },
    grid_y = DEFAULT.grid_y,
    axes = DEFAULT.axes,
    axes_len = DEFAULT.axes_len,
}

local AXIS_DEFS <const> = {
    { r = 1, g = 0, b = 0, a = 0.9, dx = 1, dy = 0, dz = 0, label = "X" },
    { r = 0, g = 1, b = 0, a = 0.9, dx = 0, dy = 1, dz = 0, label = "Y" },
    { r = 0, g = 0, b = 1, a = 0.9, dx = 0, dy = 0, dz = 1, label = "Z" },
}

local format = string.format
local floor = math.floor
local log_info = sdk.log_info
local log_warn = sdk.log_warn
local VK_TOGGLE = VK.F7

local state = {
    vertex_count = 0,
    grid_lines = 0,
    activation_count = 0,
}

local function validate_config()
    if type(cfg.grid_size) ~= "number" or cfg.grid_size < 10 or cfg.grid_size > 1000 then
        log_warn("Invalid grid_size, using default")
        cfg.grid_size = DEFAULT.grid_size
        return false
    end
    if type(cfg.grid_step) ~= "number" or cfg.grid_step < 1 or cfg.grid_step > 100 then
        log_warn("Invalid grid_step, using default")
        cfg.grid_step = DEFAULT.grid_step
        return false
    end
    if cfg.grid_step > cfg.grid_size then
        log_warn("grid_step > grid_size, adjusting")
        cfg.grid_step = math.max(1, floor(cfg.grid_size / 10))
        return false
    end
    if type(cfg.grid_color) ~= "table" or #cfg.grid_color ~= 4 then
        log_warn("Invalid grid_color, using default")
        cfg.grid_color = {
            DEFAULT.grid_color[1],
            DEFAULT.grid_color[2],
            DEFAULT.grid_color[3],
            DEFAULT.grid_color[4],
        }
        return false
    end
    for i, v in ipairs(cfg.grid_color) do
        if type(v) ~= "number" or v < 0 or v > 1 then
            log_warn(format("Invalid grid_color[%d], using default", i))
            cfg.grid_color = {
                DEFAULT.grid_color[1],
                DEFAULT.grid_color[2],
                DEFAULT.grid_color[3],
                DEFAULT.grid_color[4],
            }
            return false
        end
    end
    if type(cfg.axes_len) ~= "number" or cfg.axes_len < 1 or cfg.axes_len > 500 then
        log_warn("Invalid axes_len, using default")
        cfg.axes_len = DEFAULT.axes_len
        return false
    end
    return true
end

local function argb(r, g, b, a)
    return floor(a * 255) * 0x1000000
        + floor(r * 255) * 0x10000
        + floor(g * 255) * 0x100
        + floor(b * 255)
end

local function append_line(vertices, ax, ay, az, bx, by, bz, color)
    local n = #vertices
    vertices[n + 1], vertices[n + 2], vertices[n + 3], vertices[n + 4] =
        ax, ay, az, color
    vertices[n + 5], vertices[n + 6], vertices[n + 7], vertices[n + 8] =
        bx, by, bz, color
end

local function publish_grid()
    if not cfg.enabled then
        sdk.clear_world_lines()
        return
    end

    local vertices = {}
    local half = cfg.grid_size * 0.5
    local color = argb(table.unpack(cfg.grid_color))
    local count = 0

    for x = -half, half, cfg.grid_step do
        append_line(vertices, x, cfg.grid_y, -half, x, cfg.grid_y, half, color)
        count = count + 1
    end
    for z = -half, half, cfg.grid_step do
        append_line(vertices, -half, cfg.grid_y, z, half, cfg.grid_y, z, color)
        count = count + 1
    end
    if cfg.axes then
        for _, axis in ipairs(AXIS_DEFS) do
            append_line(
                vertices,
                0, cfg.grid_y, 0,
                axis.dx * cfg.axes_len,
                cfg.grid_y + axis.dy * cfg.axes_len,
                axis.dz * cfg.axes_len,
                argb(axis.r, axis.g, axis.b, axis.a)
            )
        end
    end

    state.grid_lines = count
    state.vertex_count = #vertices // 4
    sdk.set_world_lines(vertices)
end

sdk.on_key_down(function(vk)
    if vk ~= VK_TOGGLE then
        return false
    end
    cfg.enabled = not cfg.enabled
    log_info(format("World grid: %s", cfg.enabled and "ON" or "OFF"))
    state.activation_count = state.activation_count + 1
    return true
end)

sdk.on_frame(publish_grid)

local function draw_panel()
    TOOLS_UI.header("World Grid")
    TOOLS_UI.status_badge(cfg.enabled)
    ui.same_line()
    ui.text("grid overlay")

    TOOLS_UI.checkbox(
        "Enable Grid", cfg, "enabled",
        "Draw a reference grid and world axes in the game world"
    )
    if not cfg.enabled then
        return
    end

    if ui.collapsing_header("Grid Settings", true) then
        TOOLS_UI.slider_int(
            "Grid Size", cfg, "grid_size", 10, 500,
            "Total span of the grid in world units"
        )
        TOOLS_UI.slider_int(
            "Grid Step", cfg, "grid_step", 1, 50,
            "Distance between grid lines"
        )
        TOOLS_UI.slider_float(
            "Y Level", cfg, "grid_y", -100, 100,
            "Height at which the grid is drawn"
        )
        ui.spacing()
        TOOLS_UI.color_edit4("Grid Color", "Grid Alpha", cfg.grid_color)
    end

    if ui.collapsing_header("World Axes", true) then
        TOOLS_UI.checkbox(
            "Show Axes", cfg, "axes",
            "Draw colored X/Y/Z axes at the grid origin"
        )
        if cfg.axes then
            TOOLS_UI.slider_float(
                "Axes Length", cfg, "axes_len", 10, 200,
                "Length of each world axis line"
            )
            ui.spacing()
            ui.text("Axis Colors:")
            for _, ax in ipairs(AXIS_DEFS) do
                ui.text_colored(
                    ax.r, ax.g, ax.b, ax.a, format("  %s axis", ax.label)
                )
            end
        end
    end

    if ui.collapsing_header("Statistics", false) then
        local grid_lines = floor(cfg.grid_size / cfg.grid_step) * 2 + 2
        ui.text(format("Grid lines: ~%d", grid_lines))
        ui.text(format(
            "Vertices per frame: ~%d",
            grid_lines * 2 + (cfg.axes and 6 or 0)
        ))
        ui.text(format("Activations: %d", state.activation_count))
    end

    ui.spacing()
    TOOLS_UI.keybind("F7", "Toggle grid on / off")
end

if _G.TOOLS_UI then
    TOOLS_UI.register_panel("world_grid", "World Grid", draw_panel)
end

sdk.on_load(function()
    validate_config()
    log_info("World grid plugin loaded")
end)

sdk.on_unload(function()
    sdk.clear_world_lines()
    state.vertex_count = 0
    state.grid_lines = 0
    state.activation_count = 0
    log_info("World grid plugin unloaded")
end)

return M
