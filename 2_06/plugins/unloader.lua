-- Adds the footer info text and unload button to the shared tools window

--- Key used to toggle the overlay UI (displayed in the footer hint).
local TOGGLE_KEY_HINT = "[INSERT] to toggle ui"

sdk.on_overlay(function()
    if not ui.begin_window("airstrike 3d tools") then
        ui.end_window()
        return
    end

    ui.separator()
    ui.text_disabled(TOGGLE_KEY_HINT)

    -- Note: actual DLL unloading must be triggered from the C++ side.
    -- This button just signals the intent. The SDK's C++ code can
    -- check a shared flag, or you can expose an sdk.request_unload().
    if ui.button("unload dll") then
        sdk.log_warn("unload requested by user")
        -- If you add sdk.request_unload() to the C++ API:
        -- sdk.request_unload()
    end

    ui.end_window()
end)

sdk.on_load(function()
    sdk.log_info("unloader plugin loaded")
end)
