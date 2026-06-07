-- Airstrike 3D Tools — UI Framework v2 (minimal edition)
-- Valve-inspired dark theme, 2x scale, single unified workspace.
-- Plugins: TOOLS_UI.register_panel(id, title, draw_fn)

_G.TOOLS_UI = {
    panels = {},
    _first_frame = true,
    _fps = 0.0,
    _fps_acc = 0.0,
    _fps_cnt = 0,
}

local ACCENT = { 0.26, 0.59, 0.98, 1.0 }

-- Generic bound widget: call fn, update tbl[key] if changed, add tooltip
local function bound_widget(fn, label, tbl, key, tip, ...)
    local v, c = fn(label, tbl[key], ...)
    if c then
        tbl[key] = v
    end
    if tip then
        ui.tooltip(tip)
    end
end

-- Layout helpers
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
    local color = active and { 0.2, 1.0, 0.2, 1.0 } or { 1.0, 0.25, 0.25, 1.0 }
    ui.text_colored(
        color[1],
        color[2],
        color[3],
        color[4],
        "  " .. label .. "  "
    )
end

function TOOLS_UI.keybind(key, desc)
    ui.text_disabled(key)
    ui.same_line()
    ui.text_disabled("—")
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

-- Bound widgets
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

function TOOLS_UI.combo(label, tbl, key, options, tip)
    local idx = tbl[key] - 1
    local new_idx, c = ui.combo(label, idx, options)
    if c then
        tbl[key] = new_idx + 1
    end
    if tip then
        ui.tooltip(tip)
    end
end

function TOOLS_UI.color_edit4(label_rgb, label_alpha, color_tbl)
    local r, g, b, c =
        ui.color_edit3(label_rgb, color_tbl[1], color_tbl[2], color_tbl[3])
    if c then
        color_tbl[1], color_tbl[2], color_tbl[3] = r, g, b
    end
    color_tbl[4] = ui.slider_float(label_alpha, color_tbl[4], 0.0, 1.0)
end

function TOOLS_UI.action_button(label, tip)
    local clicked = ui.button(label)
    if tip then
        ui.tooltip(tip)
    end
    return clicked
end

-- Panel registration
function TOOLS_UI.register_panel(id, title, draw_fn)
    table.insert(TOOLS_UI.panels, { id = id, title = title, draw = draw_fn })
    table.sort(TOOLS_UI.panels, function(a, b)
        return a.title < b.title
    end)
end

-- Main workspace
sdk.on_overlay(function()
    TOOLS_UI._fps_acc = TOOLS_UI._fps_acc + ui.get_framerate()
    TOOLS_UI._fps_cnt = TOOLS_UI._fps_cnt + 1
    if TOOLS_UI._fps_cnt >= 30 then
        TOOLS_UI._fps = TOOLS_UI._fps_acc / 30
        TOOLS_UI._fps_acc, TOOLS_UI._fps_cnt = 0.0, 0
    end

    if TOOLS_UI._first_frame then
        ui.set_next_window_size(840, 960)
        TOOLS_UI._first_frame = false
    end

    if not ui.begin_window("AIRSTRIKE 3D TOOLS") then
        return
    end

    ui.text_colored(ACCENT[1], ACCENT[2], ACCENT[3], 1.0, "  AIRSTRIKE 3D")
    ui.same_line()
    ui.text_disabled("|  Toolkit")
    ui.separator()
    ui.spacing()

    if ui.tab_bar_begin("##workspace") then
        for _, p in ipairs(TOOLS_UI.panels) do
            if ui.tab_item_begin(p.title .. "  ##" .. p.id) then
                p.draw()
                ui.tab_item_end()
            end
        end
        ui.tab_bar_end()
    end

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
