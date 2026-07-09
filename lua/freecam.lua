---@meta
--- Free camera: WASD + mouse look, shared across OpenGL and Direct3D 8.
---
--- @module freecam
--- @version 3.1.0
---
--- Controls:
---   W/A/S/D       — Move
---   Space/Ctrl    — Up / down
---   Shift         — Sprint
---   Right-click   — Mouse look

local M = {}

---@class CameraState
---@field enabled boolean
---@field mouse_look boolean
---@field base_speed number
---@field sprint_mult number
---@field sensitivity number
---@field pos_x number
---@field pos_y number
---@field pos_z number
---@field yaw number
---@field pitch number
---@field cursor_save_x integer
---@field cursor_save_y integer

local DEFAULT <const> = {
    enabled = true,
    mouse_look = false,
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

---@type CameraState
local cam = {
    enabled = DEFAULT.enabled,
    mouse_look = DEFAULT.mouse_look,
    base_speed = DEFAULT.base_speed,
    sprint_mult = DEFAULT.sprint_mult,
    sensitivity = DEFAULT.sensitivity,
    pos_x = DEFAULT.pos_x,
    pos_y = DEFAULT.pos_y,
    pos_z = DEFAULT.pos_z,
    yaw = DEFAULT.yaw,
    pitch = DEFAULT.pitch,
    cursor_save_x = DEFAULT.cursor_save_x,
    cursor_save_y = DEFAULT.cursor_save_y,
}

local floor = math.floor
local is_key_down = sdk.is_key_down
local get_cursor_pos = sdk.get_cursor_pos
local set_cursor_pos = sdk.set_cursor_pos
local show_cursor = sdk.show_cursor
local get_window_rect = sdk.get_window_rect
local radians = gmath.radians
local cos = gmath.cos
local sin = gmath.sin
local normalize = gmath.normalize
local cross = gmath.cross
local clamp = gmath.clamp
local mod = gmath.mod

local VK_W, VK_A, VK_S, VK_D = VK.W, VK.A, VK.S, VK.D
local VK_SPACE, VK_CONTROL, VK_SHIFT, VK_RBUTTON =
    VK.SPACE, VK.CONTROL, VK.SHIFT, VK.RBUTTON

local was_rbutton_down = false
local live_seeded = false

local function sync_pose_from_sdk()
    cam.pos_x, cam.pos_y, cam.pos_z, cam.yaw, cam.pitch = sdk.camera_get_pose()
end

local function publish_pose()
    local ok, err = TOOLS_UI.safe_call(function()
        sdk.camera_set_pose(cam.pos_x, cam.pos_y, cam.pos_z, cam.yaw, cam.pitch)
    end)
    if not ok then
        sdk.log_error(("Failed to apply camera: %s"):format(err))
    end
end

local function basis()
    local yaw = radians(cam.yaw)
    local pitch = radians(cam.pitch)
    local cp = cos(pitch)
    local fx, fy, fz = normalize(cos(yaw) * cp, sin(pitch), sin(yaw) * cp)
    local rx, ry, rz = normalize(cross(fx, fy, fz, 0, 1, 0))
    return fx, fy, fz, rx, ry, rz
end

local function enable_mouse_look()
    cam.cursor_save_x, cam.cursor_save_y = get_cursor_pos()
    cam.mouse_look = true
    show_cursor(false)
end

local function disable_mouse_look()
    cam.mouse_look = false
    set_cursor_pos(cam.cursor_save_x, cam.cursor_save_y)
    show_cursor(true)
end

local function reset_camera()
    cam.pos_x, cam.pos_y, cam.pos_z = DEFAULT.pos_x, DEFAULT.pos_y, DEFAULT.pos_z
    cam.yaw, cam.pitch = DEFAULT.yaw, DEFAULT.pitch
    live_seeded = true
    publish_pose()
    sdk.log_info("Camera reset to default position")
end

sdk.on_frame(function()
    sdk.camera_enable(cam.enabled)
    if not cam.enabled then
        live_seeded = false
        if cam.mouse_look then
            disable_mouse_look()
        end
        return
    end

    -- Backend may adopt the live game camera once; mirror it into Lua state.
    if not live_seeded and sdk.camera_has_observed() then
        sync_pose_from_sdk()
        live_seeded = true
    end

    local rbutton = is_key_down(VK_RBUTTON)
    if rbutton and not was_rbutton_down then
        enable_mouse_look()
    elseif not rbutton and was_rbutton_down then
        disable_mouse_look()
    end
    was_rbutton_down = rbutton

    if cam.mouse_look then
        local wl, wt, wr, wb = get_window_rect()
        local cx = floor((wl + wr) * 0.5)
        local cy = floor((wt + wb) * 0.5)
        local mx, my = get_cursor_pos()
        if mx ~= cx or my ~= cy then
            cam.yaw = mod(cam.yaw + (mx - cx) * cam.sensitivity, 360.0)
            cam.pitch = clamp(
                cam.pitch - (my - cy) * cam.sensitivity, -89.0, 89.0
            )
            set_cursor_pos(cx, cy)
        end
    end

    local step = cam.base_speed
        * (is_key_down(VK_SHIFT) and cam.sprint_mult or 1)
        * ui.get_delta_time()
    local fx, fy, fz, rx, ry, rz = basis()
    local moves = {
        { VK_W, fx, fy, fz, 1 },
        { VK_S, fx, fy, fz, -1 },
        { VK_D, rx, ry, rz, 1 },
        { VK_A, rx, ry, rz, -1 },
        { VK_SPACE, 0, 1, 0, 1 },
        { VK_CONTROL, 0, 1, 0, -1 },
    }
    for i = 1, #moves do
        local key, ax, ay, az, sign = table.unpack(moves[i])
        if is_key_down(key) then
            local d = sign * step
            cam.pos_x = cam.pos_x + ax * d
            cam.pos_y = cam.pos_y + ay * d
            cam.pos_z = cam.pos_z + az * d
        end
    end
    publish_pose()
end)

local function draw_panel()
    TOOLS_UI.header("Freecam")
    TOOLS_UI.status_badge(cam.enabled)
    ui.same_line()
    ui.text("freecam state")
    ui.spacing()

    TOOLS_UI.checkbox(
        "Enable Freecam", cam, "enabled",
        "Override the game camera with custom WASD + mouse look"
    )
    if not cam.enabled then
        return
    end

    if ui.collapsing_header("Movement Settings", true) then
        TOOLS_UI.drag_float(
            "Mouse Sensitivity", cam, "sensitivity", 0.005, 0.01, 2.0,
            "Hold right-click and move mouse to look around"
        )
        TOOLS_UI.drag_float(
            "Movement Speed", cam, "base_speed", 0.5, 0.1, 1000.0,
            "WASD movement speed. Hold SHIFT to sprint"
        )
        TOOLS_UI.drag_float(
            "Sprint Multiplier", cam, "sprint_mult", 0.1, 1.0, 50.0,
            "Speed multiplier applied while holding SHIFT"
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
        ui.text(("Position: %.2f  %.2f  %.2f"):format(
            cam.pos_x, cam.pos_y, cam.pos_z
        ))
        ui.text(("Rotation: %.1f°  %.1f°"):format(cam.yaw, cam.pitch))
        ui.text(("Mouse Look: %s"):format(
            cam.mouse_look and "Active" or "Inactive"
        ))
    end

    if ui.collapsing_header("Controls", false) then
        TOOLS_UI.keybind("W/A/S/D", "Move camera")
        TOOLS_UI.keybind("Space/Ctrl", "Move up/down")
        TOOLS_UI.keybind("Shift", "Sprint (4x speed)")
        TOOLS_UI.keybind("Right-click", "Hold for mouse look")
    end

    ui.spacing()
    if ui.button("Reset Camera") then
        reset_camera()
    end
    ui.same_line()
    ui.text_disabled("Restore default position and rotation")

    if sdk.camera_is_enabled() then
        publish_pose()
    end
end

if _G.TOOLS_UI then
    TOOLS_UI.register_panel("freecam", "Freecam", draw_panel)
end

sdk.on_load(function()
    sdk.log_info("Freecam plugin loaded")
end)

sdk.on_unload(function()
    sdk.camera_enable(false)
    if cam.mouse_look then
        disable_mouse_look()
    end
    sdk.log_info("Freecam plugin unloaded")
end)

return M
