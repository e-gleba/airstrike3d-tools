-- Airstrike 2 — Cheat Codes Panel (minimal edition)

---@type { label: string, code: string, desc: string }[]
local cheats = {
    {
        label = "10 Lives",
        code = "igonnaliveforever",
        desc = "+10 extra lives",
    },
    {
        label = "All Weapons",
        code = "showmetheweapons",
        desc = "Unlock every weapon",
    },
    {
        label = "All Missiles",
        code = "moremoreweapons",
        desc = "Max missile ammo",
    },
    {
        label = "All Power-ups",
        code = "glitteringprizes",
        desc = "All power-ups active",
    },
    {
        label = "God Mode",
        code = "invulnerability",
        desc = "No damage taken",
    },
    {
        label = "Win Mission",
        code = "deadlineisnear",
        desc = "Skip to victory",
    },
    {
        label = "Lose Mission",
        code = "diediediemydarling",
        desc = "Instant failure",
    },
}

local function draw_panel()
    TOOLS_UI.header("Cheat Codes")
    ui.text_wrapped(
        "Click any button to activate the cheat in Airstrike 2. Codes are sent as keystrokes."
    )
    ui.spacing()
    ui.spacing()

    for _, cheat in ipairs(cheats) do
        if TOOLS_UI.action_button(cheat.label, cheat.code) then
            sdk.send_chars(cheat.code)
            sdk.log_info(string.format("cheat activated: %s", cheat.label))
        end
        ui.same_line()
        ui.text_disabled(cheat.desc)
    end

    ui.spacing()
    ui.separator()
    ui.text_disabled(string.format("%d cheats available", #cheats))
end

if _G.TOOLS_UI then
    TOOLS_UI.register_panel("cheats", "Cheats", draw_panel)
end

sdk.on_load(function()
    sdk.log_info(string.format("cheats plugin loaded (%d cheats)", #cheats))
end)