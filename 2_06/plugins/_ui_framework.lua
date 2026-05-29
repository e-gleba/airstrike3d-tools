-- ============================================================================
-- Airstrike 3D Tools — Professional UI Framework  v2
-- ============================================================================
-- Valve / Source-engine-inspired dark theme, 2x scaled everything.
-- Single unified workspace — no extra windows, no clutter.
--
-- Plugins register via TOOLS_UI.register_panel(id, title, draw_fn).
-- The framework renders a clean tabbed overlay with status footer.
-- ============================================================================

_G.TOOLS_UI = {
    panels = {},
    _first_frame = true,
    _fps = 0.0,
    _fps_acc = 0.0,
    _fps_cnt = 0,
}

local ACCENT_R = 0.26
local ACCENT_G = 0.59
local ACCENT_B = 0.98

-- ---------------------------------------------------------------------------
-- Layout helpers
-- ---------------------------------------------------------------------------

--- Section header with accent-colored title and separator.
function TOOLS_UI.header(text)
    ui.spacing()
    ui.text_colored(ACCENT_R, ACCENT_G, ACCENT_B, 1.0, text)
    ui.separator()
end

--- Small sub-header (less prominent).
function TOOLS_UI.subheader(text)
    ui.spacing()
    ui.text_colored(0.55, 0.55, 0.60, 1.0, text)
end

--- Inline status indicator: [ON] or [OFF]
function TOOLS_UI.status_badge(active, on_label, off_label)
    on_label = on_label or "ON"
    off_label = off_label or "OFF"
    if active then
        ui.text_colored(0.2, 1.0, 0.2, 1.0, "  " .. on_label .. "  ")
    else
        ui.text_colored(1.0, 0.25, 0.25, 1.0, "  " .. off_label .. "  ")
    end
end

--- Display a keybind hint.
function TOOLS_UI.keybind(key, desc)
    ui.text_disabled(key)
    ui.same_line()
    ui.text_disabled("—")
    ui.same_line()
    ui.text(desc)
end

--- Group box: draw a bordered frame around child content.
function TOOLS_UI.group_begin(label)
    ui.push_style_var_float(5, 4.0)  -- ImGuiStyleVar_FrameRounding
    ui.push_style_color(5, 0.16, 0.16, 0.20, 0.55)  -- ImGuiCol_Border
    ui.begin_group()
    ui.text_colored(ACCENT_R, ACCENT_G, ACCENT_B, 1.0, label)
    ui.spacing()
end

function TOOLS_UI.group_end()
    ui.end_group()
    ui.pop_style_var()
    ui.pop_style_color()
end

-- ---------------------------------------------------------------------------
-- Bound widgets (value + optional tooltip in one call)
-- ---------------------------------------------------------------------------

function TOOLS_UI.checkbox(label, tbl, key, tip)
    local v, c = ui.checkbox(label, tbl[key])
    if c then tbl[key] = v end
    if tip then ui.tooltip(tip) end
end

function TOOLS_UI.slider_float(label, tbl, key, min, max, tip)
    local v, c = ui.slider_float(label, tbl[key], min, max)
    if c then tbl[key] = v end
    if tip then ui.tooltip(tip) end
end

function TOOLS_UI.drag_float(label, tbl, key, speed, min, max, tip)
    local v, c = ui.drag_float(label, tbl[key], speed, min, max)
    if c then tbl[key] = v end
    if tip then ui.tooltip(tip) end
end

function TOOLS_UI.slider_int(label, tbl, key, min, max, tip)
    local v, c = ui.slider_int(label, tbl[key], min, max)
    if c then tbl[key] = v end
    if tip then ui.tooltip(tip) end
end

function TOOLS_UI.combo(label, tbl, key, options, tip)
    local idx = tbl[key] - 1
    local new_idx, c = ui.combo(label, idx, options)
    if c then tbl[key] = new_idx + 1 end
    if tip then ui.tooltip(tip) end
end

function TOOLS_UI.color_edit4(label_rgb, label_alpha, color_tbl)
    local r, g, b, c = ui.color_edit3(label_rgb, color_tbl[1], color_tbl[2], color_tbl[3])
    if c then
        color_tbl[1] = r
        color_tbl[2] = g
        color_tbl[3] = b
    end
    color_tbl[4] = ui.slider_float(label_alpha, color_tbl[4], 0.0, 1.0)
end

--- Full-width action button with accent color on hover.
function TOOLS_UI.action_button(label, tip)
    local clicked = ui.button(label)
    if tip then ui.tooltip(tip) end
    return clicked
end

-- ---------------------------------------------------------------------------
-- Panel registration
-- ---------------------------------------------------------------------------

function TOOLS_UI.register_panel(id, title, draw_fn)
    table.insert(TOOLS_UI.panels, { id = id, title = title, draw = draw_fn })
    table.sort(TOOLS_UI.panels, function(a, b) return a.title < b.title end)
end

-- ---------------------------------------------------------------------------
-- Main workspace window  (single, clean, roomy)
-- ---------------------------------------------------------------------------

sdk.on_overlay(function()
    -- Smooth FPS over 30 frames
    local fr = ui.get_framerate()
    TOOLS_UI._fps_acc = TOOLS_UI._fps_acc + fr
    TOOLS_UI._fps_cnt = TOOLS_UI._fps_cnt + 1
    if TOOLS_UI._fps_cnt >= 30 then
        TOOLS_UI._fps = TOOLS_UI._fps_acc / TOOLS_UI._fps_cnt
        TOOLS_UI._fps_acc = 0.0
        TOOLS_UI._fps_cnt = 0
    end

    if TOOLS_UI._first_frame then
        -- Big window: 2x scaled elements need room to breathe
        ui.set_next_window_size(840, 960)
        TOOLS_UI._first_frame = false
    end

    -- Single unified window — no popups, no extra dialogs
    if not ui.begin_window("AIRSTRIKE 3D TOOLS") then
        ui.end_window()
        return
    end

    -- Brand bar
    ui.text_colored(ACCENT_R, ACCENT_G, ACCENT_B, 1.0, "  AIRSTRIKE 3D")
    ui.same_line()
    ui.text_disabled("|  Toolkit")
    ui.separator()
    ui.spacing()

    -- Tab workspace
    if ui.tab_bar_begin("##workspace") then
        for _, p in ipairs(TOOLS_UI.panels) do
            if ui.tab_item_begin(p.title .. "  ##" .. p.id) then
                p.draw()
                ui.tab_item_end()
            end
        end
        ui.tab_bar_end()
    end

    -- Compact status footer
    ui.spacing()
    ui.separator()
    ui.text_disabled(string.format(
        "[INSERT] toggle UI  |  %.1f FPS  |  %d panels loaded",
        TOOLS_UI._fps, #TOOLS_UI.panels))

    ui.same_line()
    ui.set_cursor_pos_x(ui.get_window_width() - 120)
    if ui.button_sized("Unload", 80, 0) then
        sdk.log_warn("unload requested by user")
    end

    ui.end_window()
end)

sdk.on_load(function()
    sdk.log_info(string.format("UI framework v2 ready (%d panels)", #TOOLS_UI.panels))
end)
