-- Full freecam with WASD movement, mouse look (right-click hold), sprint

---@class CamState
---@field enabled       boolean
---@field mouse_look    boolean
---@field hook_identity boolean
---@field base_speed    number
---@field sprint_mult   number
---@field sensitivity   number
---@field pos_x         number
---@field pos_y         number
---@field pos_z         number
---@field yaw           number
---@field pitch         number
---@field cursor_save_x integer
---@field cursor_save_y integer

---@type CamState
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

-- Default values for the reset button.
local DEFAULT_POS_X = 0.0
local DEFAULT_POS_Y = 10.0
local DEFAULT_POS_Z = 0.0
local DEFAULT_YAW = -90.0
local DEFAULT_PITCH = 0.0

-- -----------------------------------------------------------------------
-- Helpers
-- -----------------------------------------------------------------------

--- Compute the normalised forward direction from yaw/pitch.
---@return number fx, number fy, number fz
local function calc_front()
    local yr = gmath.radians(cam.yaw)
    local pr = gmath.radians(cam.pitch)
    local cos_pr = gmath.cos(pr)
    return gmath.normalize(
        gmath.cos(yr) * cos_pr,
        gmath.sin(pr),
        gmath.sin(yr) * cos_pr
    )
end

--- Compute forward, right, and up basis vectors for the camera.
---@return number fx, number fy, number fz, number rx, number ry, number rz, number ux, number uy, number uz
local function calc_vectors()
    local fx, fy, fz = calc_front()
    local rx, ry, rz = gmath.normalize(gmath.cross(fx, fy, fz, 0, 1, 0))
    local ux, uy, uz = gmath.normalize(gmath.cross(rx, ry, rz, fx, fy, fz))
    return fx, fy, fz, rx, ry, rz, ux, uy, uz
end

--- Issue the glMultMatrixd(lookAt(…)) call that overrides the game camera.
local function apply_camera_transform()
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

--- Move `cam.pos_*` along an axis by a signed step.
---@param ax number  x component of the axis
---@param ay number  y component of the axis
---@param az number  z component of the axis
---@param step number signed distance
local function move_along(ax, ay, az, step)
    cam.pos_x = cam.pos_x + ax * step
    cam.pos_y = cam.pos_y + ay * step
    cam.pos_z = cam.pos_z + az * step
end

-- -----------------------------------------------------------------------
-- Input processing (called every frame)
-- -----------------------------------------------------------------------

--- Movement key → { axis selector, sign }.
--- Using a table-driven approach avoids repetitive if-blocks.
---@type { key: integer, axis: string, sign: number }[]
local move_bindings = {
    { key = VK.W, axis = "front", sign = 1 },
    { key = VK.S, axis = "front", sign = -1 },
    { key = VK.D, axis = "right", sign = 1 },
    { key = VK.A, axis = "right", sign = -1 },
    { key = VK.SPACE, axis = "up", sign = 1 },
    { key = VK.CONTROL, axis = "up", sign = -1 },
}

local function process_input()
    if not cam.enabled then
        return
    end

    -- Mouse look (while right mouse button is held)
    if cam.mouse_look then
        local wl, wt, wr, wb = sdk.get_window_rect()
        local cx = math.floor((wl + wr) * 0.5)
        local cy = math.floor((wt + wb) * 0.5)

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

    -- Movement
    local dt = ui.get_delta_time()
    local speed = cam.base_speed
    if sdk.is_key_down(VK.SHIFT) then
        speed = speed * cam.sprint_mult
    end
    local step = speed * dt

    local fx, fy, fz, rx, ry, rz = calc_vectors()

    -- Map axis name → components for the table-driven bindings.
    local axes = {
        front = { fx, fy, fz },
        right = { rx, ry, rz },
        up = { 0, 1, 0 },
    }

    for _, bind in ipairs(move_bindings) do
        if sdk.is_key_down(bind.key) then
            local a = axes[bind.axis]
            move_along(a[1], a[2], a[3], bind.sign * step)
        end
    end
end

-- -----------------------------------------------------------------------
-- Hook callbacks
-- -----------------------------------------------------------------------

-- Called every wglSwapBuffers (before overlay)
sdk.on_frame(process_input)

-- Called on glLoadIdentity when matrix mode is GL_MODELVIEW
sdk.on_gl_identity(function()
    if cam.enabled and cam.hook_identity then
        apply_camera_transform()
    end
end)

-- Called on gluLookAt — return true to suppress the original call
sdk.on_glu_lookat(function(_ex, _ey, _ez, _cx, _cy, _cz, _ux, _uy, _uz)
    if not cam.enabled then
        return false
    end
    apply_camera_transform()
    return true -- consume the original gluLookAt
end)

-- on_key_down stub (mouse buttons are polled separately below)
sdk.on_key_down(function(_vk)
    return false
end)

-- -----------------------------------------------------------------------
-- Right-click mouse look toggle (polled because WM_RBUTTONDOWN
-- doesn't go through on_key_down)
-- -----------------------------------------------------------------------
local was_rbutton_down = false

--- Release mouse look, restoring the saved cursor position.
local function release_mouse_look()
    if not cam.mouse_look then
        return
    end
    cam.mouse_look = false
    sdk.set_cursor_pos(cam.cursor_save_x, cam.cursor_save_y)
    sdk.show_cursor(true)
end

sdk.on_frame(function()
    if not cam.enabled then
        release_mouse_look()
        return
    end

    local rbutton_down = sdk.is_key_down(VK.RBUTTON)

    if rbutton_down and not was_rbutton_down then
        -- Just pressed
        cam.cursor_save_x, cam.cursor_save_y = sdk.get_cursor_pos()
        cam.mouse_look = true
        sdk.show_cursor(false)
    elseif not rbutton_down and was_rbutton_down then
        -- Just released
        release_mouse_look()
    end

    was_rbutton_down = rbutton_down
end)

-- -----------------------------------------------------------------------
-- Overlay UI
-- -----------------------------------------------------------------------

--- Helper: bind a drag_float widget to a cam field.
---@param label string
---@param field string   key into `cam`
---@param v_speed number
---@param v_min   number
---@param v_max   number
local function cam_drag_float(label, field, v_speed, v_min, v_max)
    local new_val, changed =
        ui.drag_float(label, cam[field], v_speed, v_min, v_max)
    if changed then
        cam[field] = new_val
    end
end

--- Helper: bind a checkbox widget to a cam field.
---@param label string
---@param field string
local function cam_checkbox(label, field)
    local new_val, changed = ui.checkbox(label, cam[field])
    if changed then
        cam[field] = new_val
    end
end

sdk.on_overlay(function()
    ui.set_next_window_pos(20, 20)
    ui.set_next_window_size(450, 550)

    if not ui.begin_window("airstrike 3d tools") then
        ui.end_window()
        return
    end

    if ui.collapsing_header("freecam", true) then
        cam_checkbox("enabled##cam", "enabled")

        ui.same_line()
        if cam.enabled then
            ui.text_colored(0, 1, 0, 1, "[active]")
        else
            ui.text_colored(1, 0, 0, 1, "[off]")
        end

        if cam.enabled then
            cam_drag_float("sensitivity", "sensitivity", 0.005, 0.01, 2.0)
            cam_drag_float("speed", "base_speed", 0.5, 0.1, 1000.0)
            cam_drag_float("sprint mult", "sprint_mult", 0.1, 1.0, 50.0)
            cam_checkbox("hook identity", "hook_identity")

            ui.text(
                string.format(
                    "pos: %.2f %.2f %.2f",
                    cam.pos_x,
                    cam.pos_y,
                    cam.pos_z
                )
            )
            ui.text(string.format("rot: %.1f / %.1f", cam.yaw, cam.pitch))

            if ui.button("reset camera") then
                cam.pos_x = DEFAULT_POS_X
                cam.pos_y = DEFAULT_POS_Y
                cam.pos_z = DEFAULT_POS_Z
                cam.yaw = DEFAULT_YAW
                cam.pitch = DEFAULT_PITCH
            end
        end
    end

    ui.end_window()
end)

-- -----------------------------------------------------------------------
-- Lifecycle
-- -----------------------------------------------------------------------
sdk.on_load(function()
    sdk.log_info("freecam plugin loaded")
end)

sdk.on_unload(function()
    release_mouse_look()
    sdk.log_info("freecam plugin unloaded")
end)
