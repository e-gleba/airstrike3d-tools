-- Draws a 3D reference grid and world axes in the game world

---@class GridColor
---@field [1] number  red
---@field [2] number  green
---@field [3] number  blue
---@field [4] number  alpha

---@class GridConfig
---@field enabled    boolean
---@field grid_size  integer
---@field grid_step  integer
---@field grid_color GridColor
---@field grid_y     number
---@field axes       boolean
---@field axes_len   number

---@type GridConfig
local cfg = {
    enabled = true,
    grid_size = 100,
    grid_step = 10,
    grid_color = { 0.3, 0.3, 0.3, 0.4 },
    grid_y = 0,
    axes = true,
    axes_len = 50,
}

--- Toggle key for the entire grid overlay.
local TOGGLE_VK = VK.F7

--- World-axis definitions: { label (for comments), color rgba, endpoint }.
--- Table-driven so adding a fourth axis is a one-liner.
---@type { r: number, g: number, b: number, a: number, dx: number, dy: number, dz: number }[]
local AXIS_DEFS = {
    { r = 1, g = 0, b = 0, a = 0.9, dx = 1, dy = 0, dz = 0 }, -- X = red
    { r = 0, g = 1, b = 0, a = 0.9, dx = 0, dy = 1, dz = 0 }, -- Y = green (up)
    { r = 0, g = 0, b = 1, a = 0.9, dx = 0, dy = 0, dz = 1 }, -- Z = blue
}

-- -----------------------------------------------------------------------
-- GL state helpers
-- -----------------------------------------------------------------------

--- Push a clean GL state suitable for overlay-style line drawing.
local function push_line_state()
    sdk.gl_push_attrib(GL.ALL_ATTRIB_BITS)
    sdk.gl_push_matrix()

    sdk.gl_disable(GL.DEPTH_TEST)
    sdk.gl_disable(GL.TEXTURE_2D)
    sdk.gl_disable(GL.LIGHTING)
    sdk.gl_enable(GL.BLEND)
    sdk.gl_blend_func(GL.SRC_ALPHA, GL.ONE_MINUS_SRC_ALPHA)
end

--- Pop the GL state pushed by `push_line_state`.
local function pop_line_state()
    sdk.gl_pop_matrix()
    sdk.gl_pop_attrib()
end

-- -----------------------------------------------------------------------
-- Drawing
-- -----------------------------------------------------------------------

--- Draw the flat reference grid at the configured Y level.
local function draw_grid()
    local half = cfg.grid_size * 0.5
    local step = cfg.grid_step
    local y = cfg.grid_y
    local gc = cfg.grid_color

    sdk.gl_line_width(1.0)
    sdk.gl_color4f(gc[1], gc[2], gc[3], gc[4])
    sdk.gl_begin(GL.LINES)

    -- Lines along X
    for x = -half, half, step do
        sdk.gl_vertex3f(x, y, -half)
        sdk.gl_vertex3f(x, y, half)
    end

    -- Lines along Z
    for z = -half, half, step do
        sdk.gl_vertex3f(-half, y, z)
        sdk.gl_vertex3f(half, y, z)
    end

    sdk.gl_end()
end

--- Draw coloured world-axes from the origin at the configured Y level.
local function draw_axes()
    local len = cfg.axes_len
    local y = cfg.grid_y

    sdk.gl_line_width(3.0)
    sdk.gl_begin(GL.LINES)

    for _, ax in ipairs(AXIS_DEFS) do
        sdk.gl_color4f(ax.r, ax.g, ax.b, ax.a)
        sdk.gl_vertex3f(0, y, 0)
        sdk.gl_vertex3f(ax.dx * len, y + ax.dy * len, ax.dz * len)
    end

    sdk.gl_end()
end

-- -----------------------------------------------------------------------
-- Callbacks
-- -----------------------------------------------------------------------

sdk.on_key_down(function(vk)
    if vk ~= TOGGLE_VK then
        return false
    end
    cfg.enabled = not cfg.enabled
    sdk.log_info(string.format("world grid: %s", cfg.enabled and "ON" or "OFF"))
    return true
end)

sdk.on_gl_identity(function()
    if not cfg.enabled then
        return
    end

    push_line_state()

    draw_grid()
    if cfg.axes then
        draw_axes()
    end

    pop_line_state()
end)

-- -----------------------------------------------------------------------
-- Overlay UI helpers
-- -----------------------------------------------------------------------

--- Bind a slider_int widget directly to a `cfg` field.
---@param label string
---@param field string
---@param v_min integer
---@param v_max integer
local function cfg_slider_int(label, field, v_min, v_max)
    cfg[field] = ui.slider_int(label, cfg[field], v_min, v_max)
end

--- Bind a slider_float widget directly to a `cfg` field.
---@param label string
---@param field string
---@param v_min number
---@param v_max number
local function cfg_slider_float(label, field, v_min, v_max)
    cfg[field] = ui.slider_float(label, cfg[field], v_min, v_max)
end

--- Bind a checkbox widget directly to a `cfg` field.
---@param label string
---@param field string
local function cfg_checkbox(label, field)
    cfg[field] = ui.checkbox(label, cfg[field])
end

-- -----------------------------------------------------------------------
-- Overlay UI
-- -----------------------------------------------------------------------

sdk.on_overlay(function()
    if not cfg.enabled then
        return
    end

    ui.set_next_window_pos(10, 300)
    ui.set_next_window_size(220, 0)

    if not ui.begin_window("World Grid") then
        ui.end_window()
        return
    end

    cfg_slider_int("Size", "grid_size", 10, 500)
    cfg_slider_int("Step", "grid_step", 1, 50)
    cfg_slider_float("Y Level", "grid_y", -100, 100)
    cfg_checkbox("Show Axes", "axes")
    cfg_slider_float("Axes Length", "axes_len", 10, 200)

    local r, g, b, changed = ui.color_edit3(
        "Grid Color",
        cfg.grid_color[1],
        cfg.grid_color[2],
        cfg.grid_color[3]
    )
    if changed then
        cfg.grid_color[1] = r
        cfg.grid_color[2] = g
        cfg.grid_color[3] = b
    end

    cfg.grid_color[4] =
        ui.slider_float("Grid Alpha", cfg.grid_color[4], 0.05, 1.0)

    ui.end_window()
end)

-- -----------------------------------------------------------------------
-- Lifecycle
-- -----------------------------------------------------------------------

sdk.on_load(function()
    sdk.log_info("world grid plugin loaded")
end)
