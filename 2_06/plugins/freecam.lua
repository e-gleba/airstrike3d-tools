---@meta
--- Airstrike 3D Tools — Freecam Plugin
--- Professional free-camera system with WASD movement, mouse look, and sprint.
---
--- @module freecam
--- @author Airstrike 3D Tools Team
--- @license MIT
--- @version 3.0.0
---
--- Controls:
---   W/A/S/D       — Move forward/left/back/right
---   Space/Ctrl    — Move up/down
---   Shift         — Sprint (4x speed)
---   Right-click   — Hold to enable mouse look
---
--- Hooks:
---   on_gl_identity — Injects camera transform when glLoadIdentity is called
---   on_glu_lookat  — Intercepts gluLookAt to apply custom camera

local M = {}

-- ── Configuration ───────────────────────────────────────────────────────────

---@class CameraState
---@field enabled boolean
---@field mouse_look boolean
---@field hook_identity boolean
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

---@type CameraState
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

---@type CameraState
local DEFAULT = {
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

-- ── Performance Optimizations ───────────────────────────────────────────────

-- Cache frequently accessed functions
local radians = gmath.radians
local cos = gmath.cos
local sin = gmath.sin
local normalize = gmath.normalize
local cross = gmath.cross
local clamp = gmath.clamp
local mod = gmath.mod
local floor = math.floor
local is_key_down = sdk.is_key_down
local get_cursor_pos = sdk.get_cursor_pos
local set_cursor_pos = sdk.set_cursor_pos
local show_cursor = sdk.show_cursor
local get_window_rect = sdk.get_window_rect

-- Cache constants
local VK_W = VK.W
local VK_A = VK.A
local VK_S = VK.S
local VK_D = VK.D
local VK_SPACE = VK.SPACE
local VK_CONTROL = VK.CONTROL
local VK_SHIFT = VK.SHIFT
local VK_RBUTTON = VK.RBUTTON

-- ── Vector Math ─────────────────────────────────────────────────────────────

---Compute forward, right, and up vectors from yaw/pitch
---@return number fx, number fy, number fz Forward vector
---@return number rx, number ry, number rz Right vector
---@return number ux, number uy, number uz Up vector
local function calc_vectors()
    local yr = radians(cam.yaw)
    local pr = radians(cam.pitch)
    local cos_pr = cos(pr)
    
    -- Forward vector
    local fx, fy, fz = normalize(
        cos(yr) * cos_pr,
        sin(pr),
        sin(yr) * cos_pr
    )
    
    -- Right vector (forward × up)
    local rx, ry, rz = normalize(cross(fx, fy, fz, 0, 1, 0))
    
    -- Up vector (right × forward)
    local ux, uy, uz = normalize(cross(rx, ry, rz, fx, fy, fz))
    
    return fx, fy, fz, rx, ry, rz, ux, uy, uz
end

---Publish the camera pose through the renderer-neutral SDK.
local function apply_camera()
    local ok, err = TOOLS_UI.safe_call(function()
        sdk.camera_set_pose(cam.pos_x, cam.pos_y, cam.pos_z, cam.yaw, cam.pitch)
    end)
    
    if not ok then
        sdk.log_error(string.format("Failed to apply camera: %s", tostring(err)))
    end
end

---Move camera along an axis
---@param ax number X component
---@param ay number Y component
---@param az number Z component
---@param step number Distance to move
local function move_along(ax, ay, az, step)
    cam.pos_x = cam.pos_x + ax * step
    cam.pos_y = cam.pos_y + ay * step
    cam.pos_z = cam.pos_z + az * step
end

-- ── Key Bindings ────────────────────────────────────────────────────────────

---@class MoveBinding
---@field key integer Virtual key code
---@field axis string Axis name ("front", "right", "up")
---@field sign integer Direction (1 or -1)

---@type MoveBinding[]
local move_bindings = {
    { key = VK_W,       axis = "front", sign =  1 },
    { key = VK_S,       axis = "front", sign = -1 },
    { key = VK_D,       axis = "right", sign =  1 },
    { key = VK_A,       axis = "right", sign = -1 },
    { key = VK_SPACE,   axis = "up",    sign =  1 },
    { key = VK_CONTROL, axis = "up",    sign = -1 },
}

-- ── State Management ────────────────────────────────────────────────────────

local was_rbutton_down = false
local live_seeded = false

---Enable mouse look
local function enable_mouse_look()
    cam.cursor_save_x, cam.cursor_save_y = get_cursor_pos()
    cam.mouse_look = true
    show_cursor(false)
end

---Disable mouse look
local function disable_mouse_look()
    cam.mouse_look = false
    set_cursor_pos(cam.cursor_save_x, cam.cursor_save_y)
    show_cursor(true)
end

-- ── Frame Update ────────────────────────────────────────────────────────────

sdk.on_frame(function()
    sdk.camera_enable(cam.enabled)
    if not cam.enabled then
        live_seeded = false
        if cam.mouse_look then
            disable_mouse_look()
        end
        return
    end

    -- Backend may adopt the live game camera on enable/observe (D3D8).
    -- Pull that pose once so Lua movement starts from the real view.
    if not live_seeded and sdk.camera_has_observed() then
        cam.pos_x, cam.pos_y, cam.pos_z, cam.yaw, cam.pitch =
            sdk.camera_get_pose()
        live_seeded = true
    end

    -- Right-click toggle for mouse look
    local rbutton_down = is_key_down(VK_RBUTTON)
    if rbutton_down and not was_rbutton_down then
        enable_mouse_look()
    elseif not rbutton_down and was_rbutton_down then
        disable_mouse_look()
    end
    was_rbutton_down = rbutton_down
    
    -- Mouse look: recenter cursor and compute delta
    if cam.mouse_look then
        local wl, wt, wr, wb = get_window_rect()
        local cx = floor((wl + wr) * 0.5)
        local cy = floor((wt + wb) * 0.5)
        local mx, my = get_cursor_pos()
        
        if mx ~= cx or my ~= cy then
            cam.yaw = mod(cam.yaw + (mx - cx) * cam.sensitivity, 360.0)
            cam.pitch = clamp(cam.pitch - (my - cy) * cam.sensitivity, -89.0, 89.0)
            set_cursor_pos(cx, cy)
        end
    end
    
    -- WASD movement
    local dt = ui.get_delta_time()
    local speed = cam.base_speed * (is_key_down(VK_SHIFT) and cam.sprint_mult or 1)
    local step = speed * dt
    
    local fx, fy, fz, rx, ry, rz = calc_vectors()
    
    -- Build axis lookup table
    local axes = {
        front = { fx, fy, fz },
        right = { rx, ry, rz },
        up = { 0, 1, 0 },
    }
    
    -- Process movement bindings
    for _, bind in ipairs(move_bindings) do
        if is_key_down(bind.key) then
            local axis = axes[bind.axis]
            if axis then
                move_along(axis[1], axis[2], axis[3], bind.sign * step)
            end
        end
    end
    apply_camera()
end)

-- ── UI Panel ────────────────────────────────────────────────────────────────

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
    
    -- Movement settings
    if ui.collapsing_header("Movement Settings", true) then
        TOOLS_UI.drag_float(
            "Mouse Sensitivity", cam, "sensitivity",
            0.005, 0.01, 2.0,
            "Hold right-click and move mouse to look around"
        )
        TOOLS_UI.drag_float(
            "Movement Speed", cam, "base_speed",
            0.5, 0.1, 1000.0,
            "WASD movement speed. Hold SHIFT to sprint"
        )
        TOOLS_UI.drag_float(
            "Sprint Multiplier", cam, "sprint_mult",
            0.1, 1.0, 50.0,
            "Speed multiplier applied while holding SHIFT"
        )
        TOOLS_UI.checkbox(
            "Hook Identity", cam, "hook_identity",
            "Intercept glLoadIdentity to inject the custom camera matrix"
        )
    end
    
    -- Transform
    if ui.collapsing_header("Transform", false) then
        TOOLS_UI.drag_float("Position X", cam, "pos_x", 0.5, -5000, 5000)
        TOOLS_UI.drag_float("Position Y", cam, "pos_y", 0.5, -5000, 5000)
        TOOLS_UI.drag_float("Position Z", cam, "pos_z", 0.5, -5000, 5000)
        TOOLS_UI.drag_float("Yaw", cam, "yaw", 0.5, -360, 360)
        TOOLS_UI.drag_float("Pitch", cam, "pitch", 0.5, -89, 89)
    end
    
    -- Info
    if ui.collapsing_header("Info", false) then
        ui.text(string.format(
            "Position: %.2f  %.2f  %.2f",
            cam.pos_x, cam.pos_y, cam.pos_z
        ))
        ui.text(string.format("Rotation: %.1f°  %.1f°", cam.yaw, cam.pitch))
        ui.text(string.format("Mouse Look: %s", cam.mouse_look and "Active" or "Inactive"))
    end
    
    -- Controls
    if ui.collapsing_header("Controls", false) then
        TOOLS_UI.keybind("W/A/S/D", "Move camera")
        TOOLS_UI.keybind("Space/Ctrl", "Move up/down")
        TOOLS_UI.keybind("Shift", "Sprint (4x speed)")
        TOOLS_UI.keybind("Right-click", "Hold for mouse look")
    end
    
    -- Reset
    ui.spacing()
    if ui.button("Reset Camera") then
        cam.pos_x = DEFAULT.pos_x
        cam.pos_y = DEFAULT.pos_y
        cam.pos_z = DEFAULT.pos_z
        cam.yaw = DEFAULT.yaw
        cam.pitch = DEFAULT.pitch
        sdk.log_info("Camera reset to default position")
    end
    ui.same_line()
    ui.text_disabled("Restore default position and rotation")
end

-- ── Registration ────────────────────────────────────────────────────────────

if _G.TOOLS_UI then
    TOOLS_UI.register_panel("freecam", "Freecam", draw_panel)
end

-- ── Lifecycle ───────────────────────────────────────────────────────────────

sdk.on_load(function()
    sdk.log_info("Freecam plugin loaded")
    sdk.log_info("Controls: WASD + mouse look (right-click)")
end)

sdk.on_unload(function()
    sdk.camera_enable(false)
    if cam.mouse_look then
        disable_mouse_look()
    end
    sdk.log_info("Freecam plugin unloaded")
end)

-- ── Export ──────────────────────────────────────────────────────────────────

return M
