-- ============================================================================
-- Airstrike 3D Tools — Professional UI Framework
-- ============================================================================
-- Provides a unified tabbed workspace for all tool panels. Each plugin
-- registers a draw function via TOOLS_UI.register_panel() and the framework
-- handles windowing, layout, and chrome.
--
-- Design goals:
--   • Zero game-logic code — purely presentational
--   • Consistent spacing, typography, and color hierarchy
--   • Tooltips on every interactive control
--   • Minimal boilerplate in consumer plugins
-- ============================================================================

_G.TOOLS_UI = {
    panels = {},
    _first_frame = true,
    _fps = 0.0,
    _fps_acc = 0.0,
    _fps_cnt = 0,
}

-- ---------------------------------------------------------------------------
-- Layout helpers
-- ---------------------------------------------------------------------------

function TOOLS_UI.header(text)
    ui.spacing()
    ui.text_colored(0.26, 0.59, 0.98, 1.0, text)
    ui.separator()
end

function TOOLS_UI.status_badge(active, on_label, off_label)
    on_label = on_label or "ACTIVE"
    off_label = off_label or "OFF"
    if active then
        ui.text_colored(0.2, 1.0, 0.2, 1.0, "[" .. on_label .. "]")
    else
        ui.text_colored(1.0, 0.2, 0.2, 1.0, "[" .. off_label .. "]")
    end
end

function TOOLS_UI.keybind(key, desc)
    ui.text_disabled(key)
    ui.same_line()
    ui.text("—")
    ui.same_line()
    ui.text(desc)
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
    local idx = tbl[key] - 1 -- ImGui combo is 0-based; our tables are 1-based
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

-- ---------------------------------------------------------------------------
-- Panel registration
-- ---------------------------------------------------------------------------

function TOOLS_UI.register_panel(id, title, draw_fn)
    table.insert(TOOLS_UI.panels, { id = id, title = title, draw = draw_fn })
    table.sort(TOOLS_UI.panels, function(a, b) return a.title < b.title end)
end

-- ---------------------------------------------------------------------------
-- Main workspace window
-- ---------------------------------------------------------------------------

sdk.on_overlay(function()
    -- Smooth FPS read-out over 30 frames
    local fr = ui.get_framerate()
    TOOLS_UI._fps_acc = TOOLS_UI._fps_acc + fr
    TOOLS_UI._fps_cnt = TOOLS_UI._fps_cnt + 1
    if TOOLS_UI._fps_cnt >= 30 then
        TOOLS_UI._fps = TOOLS_UI._fps_acc / TOOLS_UI._fps_cnt
        TOOLS_UI._fps_acc = 0.0
        TOOLS_UI._fps_cnt = 0
    end

    if TOOLS_UI._first_frame then
        ui.set_next_window_size(540, 640)
        TOOLS_UI._first_frame = false
    end

    if not ui.begin_window("Airstrike 3D Tools") then
        ui.end_window()
        return
    end

    -- Tab workspace
    if ui.tab_bar_begin("##workspace") then
        for _, p in ipairs(TOOLS_UI.panels) do
            if ui.tab_item_begin(p.title .. "##" .. p.id) then
                p.draw()
                ui.tab_item_end()
            end
        end
        ui.tab_bar_end()
    end

    -- Compact footer
    ui.separator()
    ui.text_disabled(string.format("[INSERT] toggle UI   |   %.1f FPS   |   %d panels",
        TOOLS_UI._fps, #TOOLS_UI.panels))

    ui.same_line()

    if ui.button_sized("Unload", 80, 0) then
        sdk.log_warn("unload requested by user")
    end

    ui.end_window()
end)

sdk.on_load(function()
    sdk.log_info(string.format("UI framework ready (%d panels)", #TOOLS_UI.panels))
end)
