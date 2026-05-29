-- ============================================================================
-- Airstrike 2 — Cheat Codes Panel
-- ============================================================================
-- Professional cheat code injector for Airstrike 2.
-- ============================================================================

---@type { label: string, code: string, desc: string }[]
local cheats = {
    { label = "10 Lives",        code = "igonnaliveforever",     desc = "+10 extra lives" },
    { label = "All Weapons",     code = "showmetheweapons",      desc = "Unlock every weapon" },
    { label = "All Missiles",    code = "moremoreweapons",       desc = "Max missile ammo" },
    { label = "All Power-ups",   code = "glitteringprizes",      desc = "All power-ups active" },
    { label = "God Mode",        code = "invulnerability",       desc = "No damage taken" },
    { label = "Win Mission",     code = "deadlineisnear",        desc = "Skip to victory" },
    { label = "Lose Mission",    code = "diediediemydarling",    desc = "Instant failure" },
}

--- Activate a cheat by sending its key sequence.
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
    ui.text_wrapped("Click any button to activate the cheat in Airstrike 2. Codes are sent as keystrokes.")
    ui.spacing()
    ui.spacing()

    -- Two-column cheat grid
    for i, cheat in ipairs(cheats) do
        if TOOLS_UI.action_button(cheat.label, cheat.code) then
            activate_cheat(cheat)
        end
        ui.same_line()
        ui.text_disabled(cheat.desc)

        -- Add spacing between rows (every 2nd item)
        if i % 2 == 0 then
            ui.spacing()
        end
    end

    ui.spacing()
    ui.separator()
    ui.text_disabled(string.format("%d cheats available", #cheats))
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
