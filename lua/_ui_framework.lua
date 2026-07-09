---@meta
--- Airstrike 3D Tools — UI Framework v3
--- Professional ImGui-based UI framework with error handling and type safety.
---
--- @module ui_framework
--- @author Airstrike 3D Tools Team
--- @license MIT
--- @version 3.0.0
---
--- Plugin API:
---   TOOLS_UI.register_panel(id, title, draw_fn)
---   TOOLS_UI.unregister_panel(id)
---   TOOLS_UI.header(text)
---   TOOLS_UI.subheader(text)
---   TOOLS_UI.status_badge(active, on_label?, off_label?)
---   TOOLS_UI.keybind(key, desc)
---   TOOLS_UI.checkbox(label, tbl, key, tip?)
---   TOOLS_UI.slider_float(label, tbl, key, min, max, tip?)
---   TOOLS_UI.drag_float(label, tbl, key, speed, min, max, tip?)
---   TOOLS_UI.slider_int(label, tbl, key, min, max, tip?)
---   TOOLS_UI.color_edit4(label_rgb, label_alpha, color_tbl)
---   TOOLS_UI.action_button(label, tip?) -> bool
---   TOOLS_UI.safe_call(fn, ...) -> success, result

local M = {}

-- ── Theme Configuration ─────────────────────────────────────────────────────

---@type table<string, number[]>
local THEME = {
    ACCENT    = { 0.26, 0.59, 0.98, 1.0 },
    ON_COLOR  = { 0.20, 1.00, 0.20, 1.0 },
    OFF_COLOR = { 1.00, 0.25, 0.25, 1.0 },
    MUTED     = { 0.55, 0.55, 0.60, 1.0 },
    BG_GROUP  = { 0.16, 0.16, 0.20, 0.55 },
}

-- ── State Management ────────────────────────────────────────────────────────

---@class UIState
---@field panels table<string, {id: string, title: string, draw: fun()}>
---@field first_frame boolean
---@field fps number
---@field fps_accum number
---@field fps_count integer
---@field window_width integer
---@field window_height integer
local state = {
    panels = {},
    first_frame = true,
    fps = 0.0,
    fps_accum = 0.0,
    fps_count = 0,
    window_width = 840,
    window_height = 960,
}

-- ── Error Handling ──────────────────────────────────────────────────────────

---Safely execute a function with error logging
---@generic T
---@param fn fun(...): T
---@param ... any
---@return boolean success
---@return T|nil result
---@return string|nil error_msg
local function safe_call(fn, ...)
    local ok, result = xpcall(fn, debug.traceback, ...)
    if not ok then
        local error_msg = tostring(result)
        sdk.log_error(string.format("UI error: %s", error_msg))
        return false, nil, error_msg
    end
    return true, result
end

-- ── Validation Helpers ──────────────────────────────────────────────────────

---@param value any
---@param expected_type string
---@param name string
---@return boolean
local function validate_type(value, expected_type, name)
    if type(value) ~= expected_type then
        sdk.log_error(string.format("Invalid %s: expected %s, got %s",
            name, expected_type, type(value)))
        return false
    end
    return true
end

---@param tbl table
---@param key string
---@return boolean
local function validate_table_key(tbl, key)
    if type(tbl) ~= "table" then
        sdk.log_error("Invalid table parameter")
        return false
    end
    if tbl[key] == nil then
        sdk.log_error(string.format("Table key '%s' does not exist", key))
        return false
    end
    return true
end

-- ── Performance Optimizations ───────────────────────────────────────────────

-- Cache frequently accessed globals
local format = string.format
local insert = table.insert
local sort = table.sort
local ipairs = ipairs
local pairs = pairs

-- ── Layout Helpers ──────────────────────────────────────────────────────────

---Render a styled header
---@param text string
function M.header(text)
    if not validate_type(text, "string", "header text") then return end
    ui.spacing()
    local accent = THEME.ACCENT
    ui.text_colored(accent[1], accent[2], accent[3], accent[4], text)
    ui.separator()
end

---Render a styled subheader
---@param text string
function M.subheader(text)
    if not validate_type(text, "string", "subheader text") then return end
    ui.spacing()
    local muted = THEME.MUTED
    ui.text_colored(muted[1], muted[2], muted[3], muted[4], text)
end

---Render a status badge
---@param active boolean
---@param on_label? string
---@param off_label? string
function M.status_badge(active, on_label, off_label)
    local label = active and (on_label or "ON") or (off_label or "OFF")
    local color = active and THEME.ON_COLOR or THEME.OFF_COLOR
    ui.text_colored(color[1], color[2], color[3], color[4], format("  %s  ", label))
end

---Render a keybind display
---@param key string
---@param desc string
function M.keybind(key, desc)
    if not validate_type(key, "string", "key") then return end
    if not validate_type(desc, "string", "desc") then return end
    ui.text_disabled(key)
    ui.same_line()
    ui.text_disabled("\226\128\148")  -- em-dash
    ui.same_line()
    ui.text(desc)
end

---Begin a styled group
---@param label string
function M.group_begin(label)
    if not validate_type(label, "string", "label") then return end
    local bg = THEME.BG_GROUP
    ui.push_style_var_float(5, 4.0)
    ui.push_style_color(5, bg[1], bg[2], bg[3], bg[4])
    ui.begin_group()
    local accent = THEME.ACCENT
    ui.text_colored(accent[1], accent[2], accent[3], accent[4], label)
    ui.spacing()
end

---End a styled group
function M.group_end()
    ui.end_group()
    ui.pop_style_var()
    ui.pop_style_color()
end

-- ── Bound Widgets ───────────────────────────────────────────────────────────

---Generic bound widget helper
---@generic T
---@param fn fun(label: string, value: T, ...): T, boolean
---@param label string
---@param tbl table
---@param key string
---@param tip? string
---@param ... any
local function bound_widget(fn, label, tbl, key, tip, ...)
    if not validate_table_key(tbl, key) then return end
    
    local value, changed = fn(label, tbl[key], ...)
    if changed then
        tbl[key] = value
    end
    if tip then
        ui.tooltip(tip)
    end
end

---Checkbox bound to table field
---@param label string
---@param tbl table
---@param key string
---@param tip? string
function M.checkbox(label, tbl, key, tip)
    bound_widget(ui.checkbox, label, tbl, key, tip)
end

---Float slider bound to table field
---@param label string
---@param tbl table
---@param key string
---@param min number
---@param max number
---@param tip? string
function M.slider_float(label, tbl, key, min, max, tip)
    if not validate_type(min, "number", "min") then return end
    if not validate_type(max, "number", "max") then return end
    bound_widget(ui.slider_float, label, tbl, key, tip, min, max)
end

---Float drag bound to table field
---@param label string
---@param tbl table
---@param key string
---@param speed number
---@param min number
---@param max number
---@param tip? string
function M.drag_float(label, tbl, key, speed, min, max, tip)
    if not validate_type(speed, "number", "speed") then return end
    if not validate_type(min, "number", "min") then return end
    if not validate_type(max, "number", "max") then return end
    bound_widget(ui.drag_float, label, tbl, key, tip, speed, min, max)
end

---Integer slider bound to table field
---@param label string
---@param tbl table
---@param key string
---@param min integer
---@param max integer
---@param tip? string
function M.slider_int(label, tbl, key, min, max, tip)
    if not validate_type(min, "number", "min") then return end
    if not validate_type(max, "number", "max") then return end
    bound_widget(ui.slider_int, label, tbl, key, tip, min, max)
end

---Color editor with separate RGB and alpha controls
---@param label_rgb string
---@param label_alpha string
---@param color_tbl number[]
function M.color_edit4(label_rgb, label_alpha, color_tbl)
    if not validate_type(color_tbl, "table", "color_tbl") then return end
    if #color_tbl < 4 then
        sdk.log_error("color_tbl must have at least 4 elements")
        return
    end
    
    local r, g, b, changed = ui.color_edit3(
        label_rgb, color_tbl[1], color_tbl[2], color_tbl[3])
    if changed then
        color_tbl[1], color_tbl[2], color_tbl[3] = r, g, b
    end
    
    local a, a_changed = ui.slider_float(label_alpha, color_tbl[4], 0.0, 1.0)
    if a_changed then
        color_tbl[4] = a
    end
end

---Action button with optional tooltip
---@param label string
---@param tip? string
---@return boolean clicked
function M.action_button(label, tip)
    if not validate_type(label, "string", "label") then return false end
    local clicked = ui.button(label)
    if tip then
        ui.tooltip(tip)
    end
    return clicked
end

-- ── Panel Management ────────────────────────────────────────────────────────

---Register a panel
---@param id string
---@param title string
---@param draw_fn fun()
---@return boolean success
function M.register_panel(id, title, draw_fn)
    if not validate_type(id, "string", "id") then return false end
    if not validate_type(title, "string", "title") then return false end
    if not validate_type(draw_fn, "function", "draw_fn") then return false end
    
    -- Check for duplicate
    if state.panels[id] then
        sdk.log_warn(format("Panel '%s' already registered, updating", id))
    end
    
    state.panels[id] = {
        id = id,
        title = title,
        draw = draw_fn,
    }
    
    sdk.log_info(format("Registered panel: %s", title))
    return true
end

---Unregister a panel
---@param id string
---@return boolean success
function M.unregister_panel(id)
    if not validate_type(id, "string", "id") then return false end
    
    if state.panels[id] then
        state.panels[id] = nil
        sdk.log_info(format("Unregistered panel: %s", id))
        return true
    end
    
    sdk.log_warn(format("Panel '%s' not found", id))
    return false
end

---Get sorted panel list
---@return {id: string, title: string, draw: fun()}[]
local function get_sorted_panels()
    local panels = {}
    for _, panel in pairs(state.panels) do
        insert(panels, panel)
    end
    sort(panels, function(a, b)
        return a.title < b.title
    end)
    return panels
end

-- ── FPS Tracking ────────────────────────────────────────────────────────────

---Update FPS counter with moving average
local function update_fps()
    local current_fps = ui.get_framerate()
    state.fps_accum = state.fps_accum + current_fps
    state.fps_count = state.fps_count + 1
    
    -- Update every 30 frames
    if state.fps_count >= 30 then
        state.fps = state.fps_accum / 30
        state.fps_accum = 0.0
        state.fps_count = 0
    end
end

-- ── Main Overlay ────────────────────────────────────────────────────────────

sdk.on_overlay(function()
    -- Update FPS
    update_fps()
    
    -- Initial window size
    if state.first_frame then
        ui.set_next_window_size(state.window_width, state.window_height)
        state.first_frame = false
    end
    
    if not ui.begin_window("AIRSTRIKE 3D TOOLS") then
        return
    end
    
    -- Title bar
    local accent = THEME.ACCENT
    ui.text_colored(accent[1], accent[2], accent[3], accent[4], "  AIRSTRIKE 3D")
    ui.same_line()
    ui.text_disabled("|  Toolkit v3")
    ui.separator()
    ui.spacing()
    
    -- Tab workspace
    if ui.tab_bar_begin("##workspace") then
        local panels = get_sorted_panels()
        for _, panel in ipairs(panels) do
            local tab_id = format("%s  ##%s", panel.title, panel.id)
            if ui.tab_item_begin(tab_id) then
                -- Safe panel execution
                safe_call(panel.draw)
                ui.tab_item_end()
            end
        end
        ui.tab_bar_end()
    end
    
    -- Status bar
    ui.spacing()
    ui.separator()
    
    local panel_count = 0
    for _ in pairs(state.panels) do
        panel_count = panel_count + 1
    end
    
    ui.text_disabled(format(
        "[INSERT] toggle UI  |  %.1f FPS  |  %d panels",
        state.fps,
        panel_count
    ))
    
    ui.same_line()
    ui.set_cursor_pos_x(ui.get_window_width() - 120)
    if ui.button_sized("Unload", 80, 0) then
        sdk.log_warn("Unload requested by user")
    end
    
    ui.end_window()
end)

-- ── Lifecycle ───────────────────────────────────────────────────────────────

sdk.on_load(function()
    local panel_count = 0
    for _ in pairs(state.panels) do
        panel_count = panel_count + 1
    end
    sdk.log_info(format("UI framework v3 ready (%d panels)", panel_count))
end)

sdk.on_unload(function()
    -- Cleanup
    state.panels = {}
    sdk.log_info("UI framework unloaded")
end)

-- ── Export ──────────────────────────────────────────────────────────────────

-- Expose safe_call for plugins
M.safe_call = safe_call

-- Global registration
_G.TOOLS_UI = M

return M
