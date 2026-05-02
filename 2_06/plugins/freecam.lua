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
---@type { key: integer, axis: string, sign: number }[]
local move_bindings = {
    { key = VK.W,       axis = "front", sign = 1 },
    { key = VK.S,       axis = "front", sign = -1 },
    { key = VK.D,       axis = "right", sign = 1 },
    { key = VK.A,       axis = "right", sign = -1 },
    { key = VK.SPACE,   axis = "up",    sign = 1 },
    { key = VK.CONTROL, axis = "up",    sign = -1 },
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

    local axes = {
        front = { fx, fy, fz },
        right = { rx, ry, rz },
        up    = { 0, 1, 0 },
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

sdk.on_frame(process_input)

sdk.on_gl_identity(function()
    if cam.enabled and cam.hook_identity then
        apply_camera_transform()
    end
end)

sdk.on_glu_lookat(function(_ex, _ey, _ez, _cx, _cy, _cz, _ux, _uy, _uz)
    if not cam.enabled then
        return false
    end
    apply_camera_transform()
    return true
end)

sdk.on_key_down(function(_vk)
    return false
end)

-- -----------------------------------------------------------------------
-- Right-click mouse look toggle
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
        cam.cursor_save_x, cam.cursor_save_y = sdk.get_cursor_pos()
        cam.mouse_look = true
        sdk.show_cursor(false)
    elseif not rbutton_down and was_rbutton_down then
        release_mouse_look()
    end

    was_rbutton_down = rbutton_down
end)

-- -----------------------------------------------------------------------
-- UI Panel
-- -----------------------------------------------------------------------

local function draw_panel()
    TOOLS_UI.header("Freecam")
    TOOLS_UI.status_badge(cam.enabled)
    ui.same_line()
    ui.text("freecam state")
    ui.spacing()

    TOOLS_UI.checkbox("Enable Freecam", cam, "enabled",
        "Override the game camera with custom WASD + mouse look")

    if not cam.enabled then
        return
    end

    if ui.collapsing_header("Movement Settings", true) then
        TOOLS_UI.drag_float("Mouse Sensitivity", cam, "sensitivity", 0.005, 0.01, 2.0,
            "Hold right-click and move mouse to look around")
        TOOLS_UI.drag_float("Movement Speed", cam, "base_speed", 0.5, 0.1, 1000.0,
            "WASD movement speed. Hold SHIFT to sprint")
        TOOLS_UI.drag_float("Sprint Multiplier", cam, "sprint_mult", 0.1, 1.0, 50.0,
            "Speed multiplier applied while holding SHIFT")
        TOOLS_UI.checkbox("Hook Identity", cam, "hook_identity",
            "Intercept glLoadIdentity to inject the custom camera matrix")
    end

    if ui.collapsing_header("Transform", false) then
        TOOLS_UI.drag_float("Position X", cam, "pos_x", 0.5, -5000, 5000)
        TOOLS_UI.drag_float("Position Y", cam, "pos_y", 0.5, -5000, 5000)
        TOOLS_UI.drag_float("Position Z", cam, "pos_z", 0.5, -5000, 5000)
        TOOLS_UI.drag_float("Yaw", cam, "yaw", 0.5, -360, 360)
        TOOLS_UI.drag_float("Pitch", cam, "pitch", 0.5, -89, 89)
    end

    if ui.collapsing_header("Info", false) then
        ui.text(string.format("Position: %.2f  %.2f  %.2f", cam.pos_x, cam.pos_y, cam.pos_z))
        ui.text(string.format("Rotation: %.1f°  %.1f°", cam.yaw, cam.pitch))
    end

    if ui.button("Reset Camera") then
        cam.pos_x = DEFAULT_POS_X
        cam.pos_y = DEFAULT_POS_Y
        cam.pos_z = DEFAULT_POS_Z
        cam.yaw = DEFAULT_YAW
        cam.pitch = DEFAULT_PITCH
    end
    ui.same_line()
    ui.text_disabled("Restore default position and rotation")
end

if _G.TOOLS_UI then
    TOOLS_UI.register_panel("freecam", "Freecam", draw_panel)
end

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
