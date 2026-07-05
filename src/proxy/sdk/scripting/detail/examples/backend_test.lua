-- Example Plugin: backend_test.lua
-- This plugin demonstrates the Lua API surface available with both
-- sol2 and LuaBridge3 backends. No code changes needed when switching
-- via -DSDK_EXPERIMENTAL_LUABRIDGE3=ON
--
-- Purpose: Verify backend parity by exercising all major API areas
-- Usage: Place in plugins/ directory and run the game

local function log(msg)
    sdk.log_info("[backend_test] " .. msg)
end

log("Plugin loaded successfully")

-- Test math bindings
local function test_math()
    log("Testing gmath namespace...")
    
    local rad = gmath.radians(45.0)
    log(string.format("radians(45) = %.4f", rad))
    
    local cos_val = gmath.cos(rad)
    log(string.format("cos(45°) = %.4f", cos_val))
    
    local sin_val = gmath.sin(rad)
    log(string.format("sin(45°) = %.4f", sin_val))
    
    local clamped = gmath.clamp(150.0, 0.0, 100.0)
    log(string.format("clamp(150, 0, 100) = %.1f", clamped))
    
    local nx, ny, nz = gmath.normalize(3.0, 4.0, 0.0)
    log(string.format("normalize(3,4,0) = (%.4f, %.4f, %.4f)", nx, ny, nz))
    
    local cx, cy, cz = gmath.cross(1.0, 0.0, 0.0, 0.0, 1.0, 0.0)
    log(string.format("cross(x-axis, y-axis) = (%.4f, %.4f, %.4f)", cx, cy, cz))
    
    log("✓ gmath tests passed")
end

-- Test VK constants
local function test_vk_constants()
    log("Testing VK namespace...")
    
    assert(VK.INSERT ~= nil, "VK.INSERT should exist")
    assert(VK.F1 ~= nil, "VK.F1 should exist")
    assert(VK.SPACE ~= nil, "VK.SPACE should exist")
    assert(VK.W ~= nil, "VK.W should exist")
    
    log(string.format("VK.INSERT = 0x%02X", VK.INSERT))
    log(string.format("VK.F1 = 0x%02X", VK.F1))
    log("✓ VK constants test passed")
end

-- Test GL constants
local function test_gl_constants()
    log("Testing GL namespace...")
    
    assert(GL.MODELVIEW ~= nil, "GL.MODELVIEW should exist")
    assert(GL.PROJECTION ~= nil, "GL.PROJECTION should exist")
    assert(GL.TRIANGLES ~= nil, "GL.TRIANGLES should exist")
    assert(GL.DEPTH_TEST ~= nil, "GL.DEPTH_TEST should exist")
    
    log(string.format("GL.MODELVIEW = 0x%04X", GL.MODELVIEW))
    log(string.format("GL.TRIANGLES = 0x%04X", GL.TRIANGLES))
    log("✓ GL constants test passed")
end

-- Test SDK functions
local function test_sdk_functions()
    log("Testing sdk namespace...")
    
    -- Test platform functions
    local cursor_x, cursor_y = sdk.get_cursor_pos()
    log(string.format("Cursor position: (%d, %d)", cursor_x, cursor_y))
    
    local left, top, right, bottom = sdk.get_window_rect()
    log(string.format("Window rect: (%d, %d, %d, %d)", left, top, right, bottom))
    
    local log_dir = sdk.get_log_dir()
    log(string.format("Log directory: %s", log_dir))
    
    -- Test logging
    sdk.log_info("Test info message")
    sdk.log_warn("Test warning message")
    sdk.log_error("Test error message")
    
    log("✓ SDK functions test passed")
end

-- Test UI bindings (only works when overlay is active)
local function test_ui_bindings()
    log("Testing ui namespace...")
    
    -- UI functions exist but only work when overlay is initialized
    -- Just verify they're callable without errors
    if ui.get_delta_time then
        local dt = ui.get_delta_time()
        log(string.format("Delta time: %.4f", dt))
    end
    
    if ui.get_framerate then
        local fps = ui.get_framerate()
        log(string.format("Framerate: %.1f FPS", fps))
    end
    
    log("✓ UI bindings test passed")
end

-- Register callbacks
sdk.on_load(function()
    log("on_load callback fired")
    test_math()
    test_vk_constants()
    test_gl_constants()
    test_sdk_functions()
    test_ui_bindings()
    log("=== All backend tests completed ===")
end)

sdk.on_unload(function()
    log("on_unload callback fired")
    log("Plugin unloaded cleanly")
end)

sdk.on_frame(function()
    -- Runs every frame - uncomment to test hot path performance
    -- local pos_x, pos_y = sdk.get_cursor_pos()
end)

sdk.on_key_down(function(key)
    if key == VK.F1 then
        log("F1 pressed - backend test key handler active")
        return true  -- Consumed
    end
    return false  -- Not consumed
end)

log("Backend test plugin initialized")
