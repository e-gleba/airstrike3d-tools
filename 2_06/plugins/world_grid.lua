-- Draws a 3D reference grid and world axes in the game world

local cfg = {
    enabled = true,
    grid_size = 100,
    grid_step = 10,
    grid_color = { 0.3, 0.3, 0.3, 0.4 },
    grid_y = 0,
    axes = true,
    axes_len = 50,
}

local TOGGLE_VK = VK.F7

local AXIS_DEFS = {
    { r = 1, g = 0, b = 0, a = 0.9, dx = 1, dy = 0, dz = 0 },
    { r = 0, g = 1, b = 0, a = 0.9, dx = 0, dy = 1, dz = 0 },
    { r = 0, g = 0, b = 1, a = 0.9, dx = 0, dy = 0, dz = 1 },
}

local function push_line_state()
    sdk.gl_push_attrib(GL.ALL_ATTRIB_BITS)
    sdk.gl_push_matrix()
    sdk.gl_disable(GL.DEPTH_TEST)
    sdk.gl_disable(GL.TEXTURE_2D)
    sdk.gl_disable(GL.LIGHTING)
    sdk.gl_enable(GL.BLEND)
    sdk.gl_blend_func(GL.SRC_ALPHA, GL.ONE_MINUS_SRC_ALPHA)
end

local function pop_line_state()
    sdk.gl_pop_matrix()
    sdk.gl_pop_attrib()
end

local function draw_grid()
    local half, step, y, gc =
        cfg.grid_size * 0.5, cfg.grid_step, cfg.grid_y, cfg.grid_color
    sdk.gl_line_width(1.0)
    sdk.gl_color4f(gc[1], gc[2], gc[3], gc[4])
    sdk.gl_begin(GL.LINES)
    for x = -half, half, step do
        sdk.gl_vertex3f(x, y, -half)
        sdk.gl_vertex3f(x, y, half)
    end
    for z = -half, half, step do
        sdk.gl_vertex3f(-half, y, z)
        sdk.gl_vertex3f(half, y, z)
    end
    sdk.gl_end()
end

local function draw_axes()
    local len, y = cfg.axes_len, cfg.grid_y
    sdk.gl_line_width(3.0)
    sdk.gl_begin(GL.LINES)
    for _, ax in ipairs(AXIS_DEFS) do
        sdk.gl_color4f(ax.r, ax.g, ax.b, ax.a)
        sdk.gl_vertex3f(0, y, 0)
        sdk.gl_vertex3f(ax.dx * len, y + ax.dy * len, ax.dz * len)
    end
    sdk.gl_end()
end

sdk.on_key_down(function(vk)
    if vk == TOGGLE_VK then
        cfg.enabled = not cfg.enabled
        sdk.log_info(
            string.format("world grid: %s", cfg.enabled and "ON" or "OFF")
        )
        return true
    end
    return false
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

local function draw_panel()
    TOOLS_UI.header("World Grid")
    TOOLS_UI.status_badge(cfg.enabled)
    ui.same_line()
    ui.text("grid overlay")
    TOOLS_UI.checkbox(
        "Enable Grid",
        cfg,
        "enabled",
        "Draw a reference grid and world axes in the game world"
    )

    if not cfg.enabled then
        return
    end

    TOOLS_UI.slider_int(
        "Grid Size",
        cfg,
        "grid_size",
        10,
        500,
        "Total span of the grid in world units"
    )
    TOOLS_UI.slider_int(
        "Grid Step",
        cfg,
        "grid_step",
        1,
        50,
        "Distance between grid lines"
    )
    TOOLS_UI.slider_float(
        "Y Level",
        cfg,
        "grid_y",
        -100,
        100,
        "Height at which the grid is drawn"
    )
    TOOLS_UI.checkbox(
        "Show Axes",
        cfg,
        "axes",
        "Draw colored X/Y/Z axes at the grid origin"
    )
    TOOLS_UI.slider_float(
        "Axes Length",
        cfg,
        "axes_len",
        10,
        200,
        "Length of each world axis line"
    )
    ui.spacing()
    TOOLS_UI.color_edit4("Grid Color", "Grid Alpha", cfg.grid_color)
    ui.spacing()
    TOOLS_UI.keybind("F7", "Toggle grid on / off")
end

if _G.TOOLS_UI then
    TOOLS_UI.register_panel("world_grid", "World Grid", draw_panel)
end

sdk.on_load(function()
    sdk.log_info("world grid plugin loaded")
end)