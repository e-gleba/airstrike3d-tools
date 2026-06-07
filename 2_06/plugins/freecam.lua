-- Full freecam with WASD movement, mouse look (right-click hold), sprint

local cam = {
    enabled = true,
    mouse_look = false,
    hook_identity = true,
    base_speed = 20.0,
    sprint_mult = 4.0,
    sensitivity = 0.15,
    pos_x = 0.0,
    pos_y = 10.0,
    pos_z = 0.0,
    yaw = -90.0,
    pitch = 0.0,
    cursor_save_x = 0,
    cursor_save_y = 0,
}

local DEFAULT =
    { pos_x = 0.0, pos_y = 10.0, pos_z = 0.0, yaw = -90.0, pitch = 0.0 }

local function calc_vectors()
    local yr, pr = gmath.radians(cam.yaw), gmath.radians(cam.pitch)
    local cos_pr = gmath.cos(pr)
    local fx, fy, fz = gmath.normalize(
        gmath.cos(yr) * cos_pr,
        gmath.sin(pr),
        gmath.sin(yr) * cos_pr
    )
    local rx, ry, rz = gmath.normalize(gmath.cross(fx, fy, fz, 0, 1, 0))
    local ux, uy, uz = gmath.normalize(gmath.cross(rx, ry, rz, fx, fy, fz))
    return fx, fy, fz, rx, ry, rz, ux, uy, uz
end

local function apply_camera()
    local fx, fy, fz, _, _, _, ux, uy, uz = calc_vectors()
    sdk.gl_apply_lookat(
        cam.pos_x,
        cam.pos_y,
        cam.pos_z,
        cam.pos_x + fx,
        cam.pos_y + fy,
        cam.pos_z + fz,
        ux,
        uy,
        uz
    )
end

local function move_along(ax, ay, az, step)
    cam.pos_x, cam.pos_y, cam.pos_z =
        cam.pos_x + ax * step, cam.pos_y + ay * step, cam.pos_z + az * step
end

local move_bindings = {
    { key = VK.W, axis = "front", sign = 1 },
    { key = VK.S, axis = "front", sign = -1 },
    { key = VK.D, axis = "right", sign = 1 },
    { key = VK.A, axis = "right", sign = -1 },
    { key = VK.SPACE, axis = "up", sign = 1 },
    { key = VK.CONTROL, axis = "up", sign = -1 },
}

local was_rbutton_down = false

sdk.on_frame(function()
    if not cam.enabled then
        if cam.mouse_look then
            cam.mouse_look = false
            sdk.set_cursor_pos(cam.cursor_save_x, cam.cursor_save_y)
            sdk.show_cursor(true)
        end
        return
    end

    local rbutton_down = sdk.is_key_down(VK.RBUTTON)
    if rbutton_down and not was_rbutton_down then
        cam.cursor_save_x, cam.cursor_save_y = sdk.get_cursor_pos()
        cam.mouse_look = true
        sdk.show_cursor(false)
    elseif not rbutton_down and was_rbutton_down then
        cam.mouse_look = false
        sdk.set_cursor_pos(cam.cursor_save_x, cam.cursor_save_y)
        sdk.show_cursor(true)
    end
    was_rbutton_down = rbutton_down

    if cam.mouse_look then
        local wl, wt, wr, wb = sdk.get_window_rect()
        local cx, cy = math.floor((wl + wr) * 0.5), math.floor((wt + wb) * 0.5)
        local mx, my = sdk.get_cursor_pos()
        if mx ~= cx or my ~= cy then
            cam.yaw = gmath.mod(cam.yaw + (mx - cx) * cam.sensitivity, 360.0)
            cam.pitch = gmath.clamp(
                cam.pitch - (my - cy) * cam.sensitivity,
                -89.0,
                89.0
            )
            sdk.set_cursor_pos(cx, cy)
        end
    end

    local dt = ui.get_delta_time()
    local speed = cam.base_speed
        * (sdk.is_key_down(VK.SHIFT) and cam.sprint_mult or 1)
    local step = speed * dt
    local fx, fy, fz, rx, ry, rz = calc_vectors()
    local axes =
        { front = { fx, fy, fz }, right = { rx, ry, rz }, up = { 0, 1, 0 } }

    for _, bind in ipairs(move_bindings) do
        if sdk.is_key_down(bind.key) then
            local a = axes[bind.axis]
            move_along(a[1], a[2], a[3], bind.sign * step)
        end
    end
end)

sdk.on_gl_identity(function()
    if cam.enabled and cam.hook_identity then
        apply_camera()
    end
end)

sdk.on_glu_lookat(function()
    if not cam.enabled then
        return false
    end
    apply_camera()
    return true
end)

local function draw_panel()
    TOOLS_UI.header("Freecam")
    TOOLS_UI.status_badge(cam.enabled)
    ui.same_line()
    ui.text("freecam state")
    ui.spacing()

    TOOLS_UI.checkbox(
        "Enable Freecam",
        cam,
        "enabled",
        "Override the game camera with custom WASD + mouse look"
    )

    if not cam.enabled then
        return
    end

    if ui.collapsing_header("Movement Settings", true) then
        TOOLS_UI.drag_float(
            "Mouse Sensitivity",
            cam,
            "sensitivity",
            0.005,
            0.01,
            2.0,
            "Hold right-click and move mouse to look around"
        )
        TOOLS_UI.drag_float(
            "Movement Speed",
            cam,
            "base_speed",
            0.5,
            0.1,
            1000.0,
            "WASD movement speed. Hold SHIFT to sprint"
        )
        TOOLS_UI.drag_float(
            "Sprint Multiplier",
            cam,
            "sprint_mult",
            0.1,
            1.0,
            50.0,
            "Speed multiplier applied while holding SHIFT"
        )
        TOOLS_UI.checkbox(
            "Hook Identity",
            cam,
            "hook_identity",
            "Intercept glLoadIdentity to inject the custom camera matrix"
        )
    end

    if ui.collapsing_header("Transform", false) then
        TOOLS_UI.drag_float("Position X", cam, "pos_x", 0.5, -5000, 5000)
        TOOLS_UI.drag_float("Position Y", cam, "pos_y", 0.5, -5000, 5000)
        TOOLS_UI.drag_float("Position Z", cam, "pos_z", 0.5, -5000, 5000)
        TOOLS_UI.drag_float("Yaw", cam, "yaw", 0.5, -360, 360)
        TOOLS_UI.drag_float("Pitch", cam, "pitch", 0.5, -89, 89)
    end

    if ui.collapsing_header("Info", false) then
        ui.text(
            string.format(
                "Position: %.2f  %.2f  %.2f",
                cam.pos_x,
                cam.pos_y,
                cam.pos_z
            )
        )
        ui.text(string.format("Rotation: %.1f°  %.1f°", cam.yaw, cam.pitch))
    end

    if ui.button("Reset Camera") then
        cam.pos_x, cam.pos_y, cam.pos_z, cam.yaw, cam.pitch =
            DEFAULT.pos_x,
            DEFAULT.pos_y,
            DEFAULT.pos_z,
            DEFAULT.yaw,
            DEFAULT.pitch
    end
    ui.same_line()
    ui.text_disabled("Restore default position and rotation")
end

if _G.TOOLS_UI then
    TOOLS_UI.register_panel("freecam", "Freecam", draw_panel)
end

sdk.on_load(function()
    sdk.log_info("freecam plugin loaded")
end)
sdk.on_unload(function()
    if cam.mouse_look then
        cam.mouse_look = false
        sdk.set_cursor_pos(cam.cursor_save_x, cam.cursor_save_y)
        sdk.show_cursor(true)
    end
    sdk.log_info("freecam plugin unloaded")
end)