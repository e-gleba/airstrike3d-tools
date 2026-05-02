-- Cheat code buttons for Airstrike 2, injected via the overlay UI

---@type { label: string, code: string }[]
local cheats = {
    { label = "10 lives",        code = "igonnaliveforever" },
    { label = "All weapons",     code = "showmetheweapons" },
    { label = "All missiles",    code = "moremoreweapons" },
    { label = "All power-ups",   code = "glitteringprizes" },
    { label = "God mode",        code = "invulnerability" },
    { label = "Win mission",     code = "deadlineisnear" },
    { label = "Lose mission",    code = "diediediemydarling" },
}

--- Activate a cheat by sending its key sequence and logging the event.
---@param cheat { label: string, code: string }
local function activate_cheat(cheat)
    sdk.send_chars(cheat.code)
    sdk.log_info(string.format("cheat activated: %s", cheat.label))
end

-- ---------------------------------------------------------------------------
-- UI Panel
-- ---------------------------------------------------------------------------

local function draw_panel()
    TOOLS_UI.header("Cheat Codes")
    ui.text_wrapped("Click any button to activate the corresponding cheat in Airstrike 2.")
    ui.spacing()

    for _, cheat in ipairs(cheats) do
        if ui.button(cheat.label) then
            activate_cheat(cheat)
        end
        ui.tooltip(cheat.code)
    end
end

if _G.TOOLS_UI then
    TOOLS_UI.register_panel("cheats", "Cheats", draw_panel)
end

-- ---------------------------------------------------------------------------
-- Lifecycle
-- ---------------------------------------------------------------------------

sdk.on_load(function()
    sdk.log_info(string.format("cheats plugin loaded (%d cheats)", #cheats))
end)
