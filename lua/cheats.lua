---@meta
--- Airstrike 3D Tools — Cheat Codes Panel
--- Professional cheat code system with validation and error handling.
---
--- @module cheats
--- @author Airstrike 3D Tools Team
--- @license MIT
--- @version 2.0.0

local M = {}

-- ── Configuration ───────────────────────────────────────────────────────────

---@class CheatEntry
---@field label string Display label
---@field code string Cheat code string
---@field desc string Description
---@field cooldown? number Cooldown in seconds (optional)
---@field last_used? number Last activation timestamp

---@type CheatEntry[]
local cheats = {
    { label = "10 Lives",       code = "igonnaliveforever",   desc = "+10 extra lives",        cooldown = 1.0 },
    { label = "All Weapons",    code = "showmetheweapons",    desc = "Unlock every weapon",    cooldown = 1.0 },
    { label = "All Missiles",   code = "moremoreweapons",     desc = "Max missile ammo",       cooldown = 1.0 },
    { label = "All Power-ups",  code = "glitteringprizes",    desc = "All power-ups active",   cooldown = 1.0 },
    { label = "God Mode",       code = "invulnerability",     desc = "No damage taken",        cooldown = 2.0 },
    { label = "Win Mission",    code = "deadlineisnear",      desc = "Skip to victory",        cooldown = 5.0 },
    { label = "Lose Mission",   code = "diediediemydarling",  desc = "Instant failure",        cooldown = 5.0 },
}

-- ── State ───────────────────────────────────────────────────────────────────

---@class CheatState
---@field activation_count integer Total activations
---@field last_activation string Last cheat used
local state = {
    activation_count = 0,
    last_activation = "",
}

-- ── Validation ──────────────────────────────────────────────────────────────

---Validate cheat configuration
---@return boolean valid
local function validate_cheats()
    for i, cheat in ipairs(cheats) do
        if type(cheat.label) ~= "string" or cheat.label == "" then
            sdk.log_error(string.format("Cheat %d: invalid label", i))
            return false
        end
        if type(cheat.code) ~= "string" or cheat.code == "" then
            sdk.log_error(string.format("Cheat '%s': invalid code", cheat.label))
            return false
        end
        if type(cheat.desc) ~= "string" then
            sdk.log_error(string.format("Cheat '%s': invalid description", cheat.label))
            return false
        end
        if cheat.cooldown and (type(cheat.cooldown) ~= "number" or cheat.cooldown < 0) then
            sdk.log_error(string.format("Cheat '%s': invalid cooldown", cheat.label))
            return false
        end
    end
    return true
end

-- ── Cheat Activation ────────────────────────────────────────────────────────

---Check if cheat is on cooldown
---@param cheat CheatEntry
---@return boolean on_cooldown
---@return number remaining_seconds
local function is_on_cooldown(cheat)
    if not cheat.cooldown or not cheat.last_used then
        return false, 0
    end
    
    local elapsed = os.clock() - cheat.last_used
    if elapsed < cheat.cooldown then
        return true, cheat.cooldown - elapsed
    end
    return false, 0
end

---Activate a cheat code
---@param cheat CheatEntry
---@return boolean success
local function activate_cheat(cheat)
    -- Cooldown check
    local on_cooldown, remaining = is_on_cooldown(cheat)
    if on_cooldown then
        sdk.log_warn(string.format("Cheat '%s' on cooldown (%.1fs remaining)",
            cheat.label, remaining))
        return false
    end
    
    -- Safe execution
    local ok, err = TOOLS_UI.safe_call(function()
        sdk.send_chars(cheat.code)
    end)
    
    if not ok then
        sdk.log_error(string.format("Failed to activate cheat '%s': %s",
            cheat.label, tostring(err)))
        return false
    end
    
    -- Update state
    cheat.last_used = os.clock()
    state.activation_count = state.activation_count + 1
    state.last_activation = cheat.label
    
    sdk.log_info(string.format("Cheat activated: %s (total: %d)",
        cheat.label, state.activation_count))
    
    return true
end

-- ── Performance Optimizations ───────────────────────────────────────────────

local format = string.format
local ipairs = ipairs

-- ── UI Panel ────────────────────────────────────────────────────────────────

local function draw_panel()
    TOOLS_UI.header("Cheat Codes")
    
    ui.text_wrapped(
        "Click any button to activate the cheat in Airstrike 2. " ..
        "Codes are sent as keystrokes."
    )
    ui.spacing()
    ui.spacing()
    
    for _, cheat in ipairs(cheats) do
        local on_cooldown, remaining = is_on_cooldown(cheat)
        
        -- Button with cooldown indicator
        local button_label = cheat.label
        if on_cooldown then
            button_label = format("%s (%.1fs)", cheat.label, remaining)
        end
        
        if TOOLS_UI.action_button(button_label, cheat.code) then
            if not on_cooldown then
                activate_cheat(cheat)
            end
        end
        
        ui.same_line()
        
        -- Description with status
        if on_cooldown then
            ui.text_disabled(cheat.desc)
        else
            ui.text(cheat.desc)
        end
    end
    
    ui.spacing()
    ui.separator()
    
    -- Statistics
    ui.text_disabled(format("%d cheats available | %d activations",
        #cheats, state.activation_count))
    
    if state.last_activation ~= "" then
        ui.same_line()
        ui.text_disabled(format(" | Last: %s", state.last_activation))
    end
end

-- ── Registration ────────────────────────────────────────────────────────────

if _G.TOOLS_UI then
    TOOLS_UI.register_panel("cheats", "Cheats", draw_panel)
end

-- ── Lifecycle ───────────────────────────────────────────────────────────────

sdk.on_load(function()
    if not validate_cheats() then
        sdk.log_error("Cheat validation failed, plugin may not work correctly")
    end
    
    sdk.log_info(format("Cheats plugin loaded (%d cheats configured)", #cheats))
end)

sdk.on_unload(function()
    -- Reset state
    state.activation_count = 0
    state.last_activation = ""
    
    for _, cheat in ipairs(cheats) do
        cheat.last_used = nil
    end
    
    sdk.log_info("Cheats plugin unloaded")
end)

-- ── Export ──────────────────────────────────────────────────────────────────

return M
