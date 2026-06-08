--- Airstrike 3D Tools — UI Framework v2
--- Valve-inspired dark theme, 2x scale, single unified workspace.
---
--- Plugin API:
---   TOOLS_UI.register_panel(id, title, draw_fn)
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

-- ── Theme ────────────────────────────────────────────────────────────────────

local ACCENT = { 0.26, 0.59, 0.98, 1.0 }
local ON_COLOR  = { 0.2,  1.0,  0.2,  1.0 }
local OFF_COLOR = { 1.0,  0.25, 0.25, 1.0 }

-- ── State ────────────────────────────────────────────────────────────────────

_G.TOOLS_UI = {
    panels     = {},
    _first_frame = true,
    _fps       = 0.0,
    _fps_acc   = 0.0,
    _fps_cnt   = 0,
}

-- ── Helpers ──────────────────────────────────────────────────────────────────

--- Generic bound widget: calls fn(label, tbl[key], ...), updates tbl[key]
--- on change, and optionally shows a tooltip.
local function bound_widget(fn, label, tbl, key, tip, ...)
    local v, changed = fn(label, tbl[key], ...)
    if changed then
        tbl[key] = v
    end
    if tip then
        ui.tooltip(tip)
    end
end

-- ── Layout helpers ───────────────────────────────────────────────────────────

function TOOLS_UI.header(text)
    ui.spacing()
    ui.text_colored(ACCENT[1], ACCENT[2], ACCENT[3], 1.0, text)
    ui.separator()
end

function TOOLS_UI.subheader(text)
    ui.spacing()
    ui.text_colored(0.55, 0.55, 0.60, 1.0, text)
end

function TOOLS_UI.status_badge(active, on_label, off_label)
    local label = active and (on_label or "ON") or (off_label or "OFF")
    local color = active and ON_COLOR or OFF_COLOR
    ui.text_colored(color[1], color[2], color[3], color[4], "  " .. label .. "  ")
end

function TOOLS_UI.keybind(key, desc)
    ui.text_disabled(key)
    ui.same_line()
    ui.text_disabled("\226\128\148")  -- em-dash
    ui.same_line()
    ui.text(desc)
end

function TOOLS_UI.group_begin(label)
    ui.push_style_var_float(5, 4.0)
    ui.push_style_color(5, 0.16, 0.16, 0.20, 0.55)
    ui.begin_group()
    ui.text_colored(ACCENT[1], ACCENT[2], ACCENT[3], 1.0, label)
    ui.spacing()
end

function TOOLS_UI.group_end()
    ui.end_group()
    ui.pop_style_var()
    ui.pop_style_color()
end

-- ── Bound widgets ────────────────────────────────────────────────────────────

function TOOLS_UI.checkbox(label, tbl, key, tip)
    bound_widget(ui.checkbox, label, tbl, key, tip)
end

function TOOLS_UI.slider_float(label, tbl, key, min, max, tip)
    bound_widget(ui.slider_float, label, tbl, key, tip, min, max)
end

function TOOLS_UI.drag_float(label, tbl, key, speed, min, max, tip)
    bound_widget(ui.drag_float, label, tbl, key, tip, speed, min, max)
end

function TOOLS_UI.slider_int(label, tbl, key, min, max, tip)
    bound_widget(ui.slider_int, label, tbl, key, tip, min, max)
end

function TOOLS_UI.color_edit4(label_rgb, label_alpha, color_tbl)
    local r, g, b, changed =
        ui.color_edit3(label_rgb, color_tbl[1], color_tbl[2], color_tbl[3])
    if changed then
        color_tbl[1], color_tbl[2], color_tbl[3] = r, g, b
    end
    local a, a_changed = ui.slider_float(label_alpha, color_tbl[4], 0.0, 1.0)
    if a_changed then
        color_tbl[4] = a
    end
end

function TOOLS_UI.action_button(label, tip)
    local clicked = ui.button(label)
    if tip then
        ui.tooltip(tip)
    end
    return clicked
end

-- ── Panel registration ───────────────────────────────────────────────────────

function TOOLS_UI.register_panel(id, title, draw_fn)
    table.insert(TOOLS_UI.panels, { id = id, title = title, draw = draw_fn })
    table.sort(TOOLS_UI.panels, function(a, b)
        return a.title < b.title
    end)
end

-- ── Main workspace overlay ──────────────────────────────────────────────────

sdk.on_overlay(function()
    -- FPS averaging
    TOOLS_UI._fps_acc = TOOLS_UI._fps_acc + ui.get_framerate()
    TOOLS_UI._fps_cnt = TOOLS_UI._fps_cnt + 1
    if TOOLS_UI._fps_cnt >= 30 then
        TOOLS_UI._fps = TOOLS_UI._fps_acc / 30
        TOOLS_UI._fps_acc, TOOLS_UI._fps_cnt = 0.0, 0
    end

    -- Initial window size
    if TOOLS_UI._first_frame then
        ui.set_next_window_size(840, 960)
        TOOLS_UI._first_frame = false
    end

    if not ui.begin_window("AIRSTRIKE 3D TOOLS") then
        return
    end

    -- Title bar
    ui.text_colored(ACCENT[1], ACCENT[2], ACCENT[3], 1.0, "  AIRSTRIKE 3D")
    ui.same_line()
    ui.text_disabled("|  Toolkit")
    ui.separator()
    ui.spacing()

    -- Tab workspace
    if ui.tab_bar_begin("##workspace") then
        for _, panel in ipairs(TOOLS_UI.panels) do
            if ui.tab_item_begin(panel.title .. "  ##" .. panel.id) then
                panel.draw()
                ui.tab_item_end()
            end
        end
        ui.tab_bar_end()
    end

    -- Status bar
    ui.spacing()
    ui.separator()
    ui.text_disabled(
        string.format(
            "[INSERT] toggle UI  |  %.1f FPS  |  %d panels",
            TOOLS_UI._fps,
            #TOOLS_UI.panels
        )
    )
    ui.same_line()
    ui.set_cursor_pos_x(ui.get_window_width() - 120)
    if ui.button_sized("Unload", 80, 0) then
        sdk.log_warn("unload requested by user")
    end

    ui.end_window()
end)

sdk.on_load(function()
    sdk.log_info(
        string.format("UI framework v2 ready (%d panels)", #TOOLS_UI.panels)
    )
end)
