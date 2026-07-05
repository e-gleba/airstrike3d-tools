---@meta
--- Airstrike 3D Tools — World Grid Plugin
--- Professional 3D reference grid and world axes overlay.
---
--- @module world_grid
--- @author Airstrike 3D Tools Team
--- @license MIT
--- @version 3.0.0
---
--- Hotkey:
---   F7 — Toggle grid on/off

local M = {}

-- ── Configuration ───────────────────────────────────────────────────────────

---@class GridConfig
---@field enabled boolean
---@field grid_size integer Total grid span
---@field grid_step integer Distance between lines
---@field grid_color number[] RGBA color
---@field grid_y number Y height
---@field axes boolean Show world axes
---@field axes_len number Axis length

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

---@type GridConfig
local DEFAULT_CFG = {
    enabled = true,
    grid_size = 100,
    grid_step = 10,
    grid_color = { 0.3, 0.3, 0.3, 0.4 },
    grid_y = 0,
    axes = true,
    axes_len = 50,
}

-- ── Constants ───────────────────────────────────────────────────────────────

local VK_TOGGLE = VK.F7

---@class AxisDefinition
---@field r number Red component
---@field g number Green component
---@field b number Blue component
---@field a number Alpha component
---@field dx number X direction
---@field dy number Y direction
---@field dz number Z direction
---@field label string Axis label

---@type AxisDefinition[]
local AXIS_DEFS = {
    { r = 1, g = 0, b = 0, a = 0.9, dx = 1, dy = 0, dz = 0, label = "X" },
    { r = 0, g = 1, b = 0, a = 0.9, dx = 0, dy = 1, dz = 0, label = "Y" },
    { r = 0, g = 0, b = 1, a = 0.9, dx = 0, dy = 0, dz = 1, label = "Z" },
}

-- ── Performance Optimizations ───────────────────────────────────────────────

local format = string.format
local ipairs = ipairs
local gl_push_attrib = sdk.gl_push_attrib
local gl_pop_attrib = sdk.gl_pop_attrib
local gl_push_matrix = sdk.gl_push_matrix
local gl_pop_matrix = sdk.gl_pop_matrix
local gl_disable = sdk.gl_disable
local gl_enable = sdk.gl_enable
local gl_blend_func = sdk.gl_blend_func
local gl_line_width = sdk.gl_line_width
local gl_color4f = sdk.gl_color4f
local gl_begin = sdk.gl_begin
local gl_end = sdk.gl_end
local gl_vertex3f = sdk.gl_vertex3f
local log_info = sdk.log_info
local log_warn = sdk.log_warn

-- Cache GL constants
local GL_ALL_ATTRIB_BITS = GL.ALL_ATTRIB_BITS
local GL_DEPTH_TEST = GL.DEPTH_TEST
local GL_TEXTURE_2D = GL.TEXTURE_2D
local GL_LIGHTING = GL.LIGHTING
local GL_BLEND = GL.BLEND
local GL_SRC_ALPHA = GL.SRC_ALPHA
local GL_ONE_MINUS_SRC_ALPHA = GL.ONE_MINUS_SRC_ALPHA
local GL_LINES = GL.LINES

-- ── State Management ────────────────────────────────────────────────────────

---@class GridState
---@field vertex_count integer Total vertices drawn
---@field grid_lines integer Total grid lines
---@field activation_count integer Total activations

local state = {
    vertex_count = 0,
    grid_lines = 0,
    activation_count = 0,
}

-- ── Validation ──────────────────────────────────────────────────────────────

---Validate configuration
---@return boolean valid
local function validate_config()
    if type(cfg.grid_size) ~= "number" or cfg.grid_size < 10 or cfg.grid_size > 1000 then
        log_warn("Invalid grid_size, using default")
        cfg.grid_size = DEFAULT_CFG.grid_size
        return false
    end
    
    if type(cfg.grid_step) ~= "number" or cfg.grid_step < 1 or cfg.grid_step > 100 then
        log_warn("Invalid grid_step, using default")
        cfg.grid_step = DEFAULT_CFG.grid_step
        return false
    end
    
    if cfg.grid_step > cfg.grid_size then
        log_warn("grid_step > grid_size, adjusting")
        cfg.grid_step = math.max(1, math.floor(cfg.grid_size / 10))
        return false
    end
    
    if type(cfg.grid_color) ~= "table" or #cfg.grid_color ~= 4 then
        log_warn("Invalid grid_color, using default")
        cfg.grid_color = DEFAULT_CFG.grid_color
        return false
    end
    
    for i, v in ipairs(cfg.grid_color) do
        if type(v) ~= "number" or v < 0 or v > 1 then
            log_warn(format("Invalid grid_color[%d], using default", i))
            cfg.grid_color = DEFAULT_CFG.grid_color
            return false
        end
    end
    
    if type(cfg.axes_len) ~= "number" or cfg.axes_len < 1 or cfg.axes_len > 500 then
        log_warn("Invalid axes_len, using default")
        cfg.axes_len = DEFAULT_CFG.axes_len
        return false
    end
    
    return true
end

-- ── GL State Management ─────────────────────────────────────────────────────

---Push GL state for line rendering
local function push_line_state()
    local ok, err = TOOLS_UI.safe_call(function()
        gl_push_attrib(GL_ALL_ATTRIB_BITS)
        gl_push_matrix()
        gl_disable(GL_DEPTH_TEST)
        gl_disable(GL_TEXTURE_2D)
        gl_disable(GL_LIGHTING)
        gl_enable(GL_BLEND)
        gl_blend_func(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)
    end)
    
    if not ok then
        log_warn(format("Failed to push GL state: %s", tostring(err)))
        return false
    end
    
    return true
end

---Pop GL state after rendering
local function pop_line_state()
    local ok, err = TOOLS_UI.safe_call(function()
        gl_pop_matrix()
        gl_pop_attrib()
    end)
    
    if not ok then
        log_warn(format("Failed to pop GL state: %s", tostring(err)))
    end
end

-- ── Drawing ─────────────────────────────────────────────────────────────────

---Draw reference grid
local function draw_grid()
    local half = cfg.grid_size * 0.5
    local step = cfg.grid_step
    local y = cfg.grid_y
    local gc = cfg.grid_color
    
    local ok, err = TOOLS_UI.safe_call(function()
        gl_line_width(1.0)
        gl_color4f(gc[1], gc[2], gc[3], gc[4])
        gl_begin(GL_LINES)
        
        local line_count = 0
        
        -- Lines along X axis
        for x = -half, half, step do
            gl_vertex3f(x, y, -half)
            gl_vertex3f(x, y, half)
            line_count = line_count + 1
        end
        
        -- Lines along Z axis
        for z = -half, half, step do
            gl_vertex3f(-half, y, z)
            gl_vertex3f(half, y, z)
            line_count = line_count + 1
        end
        
        gl_end()
        
        state.grid_lines = line_count
        state.vertex_count = line_count * 2
    end)
    
    if not ok then
        log_warn(format("Failed to draw grid: %s", tostring(err)))
    end
end

---Draw world axes
local function draw_axes()
    local len = cfg.axes_len
    local y = cfg.grid_y
    
    local ok, err = TOOLS_UI.safe_call(function()
        gl_line_width(3.0)
        gl_begin(GL_LINES)
        
        for _, ax in ipairs(AXIS_DEFS) do
            gl_color4f(ax.r, ax.g, ax.b, ax.a)
            gl_vertex3f(0, y, 0)
            gl_vertex3f(ax.dx * len, y + ax.dy * len, ax.dz * len)
        end
        
        gl_end()
    end)
    
    if not ok then
        log_warn(format("Failed to draw axes: %s", tostring(err)))
    end
end

-- ── Hooks ───────────────────────────────────────────────────────────────────

sdk.on_key_down(function(vk)
    if vk == VK_TOGGLE then
        cfg.enabled = not cfg.enabled
        log_info(format("World grid: %s", cfg.enabled and "ON" or "OFF"))
        state.activation_count = state.activation_count + 1
        return true
    end
    return false
end)

sdk.on_gl_identity(function()
    if not cfg.enabled then
        return
    end
    
    if not push_line_state() then
        return
    end
    
    draw_grid()
    
    if cfg.axes then
        draw_axes()
    end
    
    pop_line_state()
end)

-- ── UI Panel ────────────────────────────────────────────────────────────────

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
    
    -- Grid settings
    if ui.collapsing_header("Grid Settings", true) then
        TOOLS_UI.slider_int(
            "Grid Size", cfg, "grid_size",
            10, 500,
            "Total span of the grid in world units"
        )
        TOOLS_UI.slider_int(
            "Grid Step", cfg, "grid_step",
            1, 50,
            "Distance between grid lines"
        )
        TOOLS_UI.slider_float(
            "Y Level", cfg, "grid_y",
            -100, 100,
            "Height at which the grid is drawn"
        )
        
        ui.spacing()
        TOOLS_UI.color_edit4("Grid Color", "Grid Alpha", cfg.grid_color)
    end
    
    -- Axes settings
    if ui.collapsing_header("World Axes", true) then
        TOOLS_UI.checkbox(
            "Show Axes", cfg, "axes",
            "Draw colored X/Y/Z axes at the grid origin"
        )
        
        if cfg.axes then
            TOOLS_UI.slider_float(
                "Axes Length", cfg, "axes_len",
                10, 200,
                "Length of each world axis line"
            )
            
            ui.spacing()
            ui.text("Axis Colors:")
            for _, ax in ipairs(AXIS_DEFS) do
                ui.text_colored(ax.r, ax.g, ax.b, ax.a, format("  %s axis", ax.label))
            end
        end
    end
    
    -- Statistics
    if ui.collapsing_header("Statistics", false) then
        local grid_lines = math.floor(cfg.grid_size / cfg.grid_step) * 2 + 2
        ui.text(format("Grid lines: ~%d", grid_lines))
        ui.text(format("Vertices per frame: ~%d", grid_lines * 2 + (cfg.axes and 6 or 0)))
        ui.text(format("Activations: %d", state.activation_count))
    end
    
    -- Hotkey reference
    ui.spacing()
    TOOLS_UI.keybind("F7", "Toggle grid on / off")
end

-- ── Registration ────────────────────────────────────────────────────────────

if _G.TOOLS_UI then
    TOOLS_UI.register_panel("world_grid", "World Grid", draw_panel)
end

-- ── Lifecycle ───────────────────────────────────────────────────────────────

sdk.on_load(function()
    validate_config()
    log_info("World grid plugin loaded")
end)

sdk.on_unload(function()
    -- Reset state
    state.vertex_count = 0
    state.grid_lines = 0
    state.activation_count = 0
    
    log_info("World grid plugin unloaded")
end)

-- ── Export ──────────────────────────────────────────────────────────────────

return M
