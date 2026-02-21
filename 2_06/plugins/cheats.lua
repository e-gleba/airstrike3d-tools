-- Cheat code buttons for Airstrike 2, injected via the overlay UI

---@type { label: string, code: string }[]
local cheats = {
    { label = "10 lives", code = "igonnaliveforever" },
    { label = "all weapons", code = "showmetheweapons" },
    { label = "all missiles", code = "moremoreweapons" },
    { label = "all power-ups", code = "glitteringprizes" },
    { label = "god mode", code = "invulnerability" },
    { label = "win mission", code = "deadlineisnear" },
    { label = "lose mission", code = "diediediemydarling" },
}

--- Activate a cheat by sending its key sequence and logging the event.
---@param cheat { label: string, code: string }
local function activate_cheat(cheat)
    sdk.send_chars(cheat.code)
    sdk.log_info(string.format("cheat activated: %s", cheat.label))
end

sdk.on_overlay(function()
    -- We share the same window opened by 01_freecam.lua.
    -- ImGui::Begin with the same title reuses the window, so we just call
    -- begin again with the same name. If freecam didn't open it, we open it.
    if not ui.begin_window("airstrike 3d tools") then
        ui.end_window()
        return
    end

    if ui.collapsing_header("cheat codes (airstrike 2)", true) then
        ui.text_wrapped("click buttons to activate cheats")
        ui.separator()

        for _, cheat in ipairs(cheats) do
            if ui.button(cheat.label) then
                activate_cheat(cheat)
            end
        end
    end

    ui.end_window()
end)

sdk.on_load(function()
    sdk.log_info(string.format("cheats plugin loaded (%d cheats)", #cheats))
end)
