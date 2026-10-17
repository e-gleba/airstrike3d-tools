---@meta
--- Texture sampling experiments (Direct3D 8 only).
---
--- @module textures
--- @version 1.0.0
---
--- Overrides per-stage MAG/MIN/MIP filters on textured 3D geometry through
--- the renderer-neutral visual API. Points/lines keep their alpha look and
--- the screen-space HUD is untouched. OpenGL builds report the
--- `texture_sampling` capability as false and ignore the mode.
---
--- Hotkey: F6 — cycle sampling modes

local M = {}

---@class TextureConfig
---@field enabled boolean
---@field mode integer 0=normal, 1=point, 2=linear, 3=anisotropic

local DEFAULT <const> = {
    enabled = true,
    mode = 0,
}

---@type TextureConfig
local cfg = {
    enabled = DEFAULT.enabled,
    mode = DEFAULT.mode,
}

local MODES <const> = {
    { name = "Normal",      desc = "Engine filter state untouched" },
    { name = "Point",       desc = "Nearest-neighbour mag/min (retro look)" },
    { name = "Linear",      desc = "Bilinear mag/min" },
    { name = "Anisotropic", desc = "Anisotropic min filter + linear magnification" },
}

local MODE_COUNT <const> = #MODES
local VK_CYCLE = VK.F6
local format = string.format
local log_info = sdk.log_info
local log_warn = sdk.log_warn

-- Binding guards: an older proxy DLL lacks the texture-sampling additions.
local set_sampling = sdk.set_texture_sampling
-- Queried live instead of cached at load: the backend is only known after
-- the first CreateDevice, long after plugins load, so a cached answer lies.
local function sampling_capable()
    return sdk.has_graphics_capability ~= nil and
        sdk.has_graphics_capability("texture_sampling") or false
end

local warned_missing = false
local published_mode

local function wanted_mode()
    if cfg.enabled then
        return cfg.mode
    end
    return 0
end

local function mode_name(mode)
    return mode ~= 0 and MODES[mode + 1].name or "off"
end

local function publish_sampling()
    local wanted = wanted_mode()
    if published_mode == wanted then
        return
    end
    if not set_sampling then
        -- Old proxy DLL without the binding: warn once, stop retrying.
        published_mode = wanted
        if not warned_missing then
            warned_missing = true
            log_warn("Texture sampling needs a rebuilt bass.dll "
                         .. "(sdk.set_texture_sampling missing)")
        end
        return
    end
    set_sampling(wanted)
    published_mode = wanted
    log_info(format("Texture sampling: %s", mode_name(wanted)))
end

sdk.on_frame(publish_sampling)

sdk.on_key_down(function(vk)
    if vk ~= VK_CYCLE or not cfg.enabled then
        return false
    end
    cfg.mode = (cfg.mode + 1) % MODE_COUNT
    log_info(format("Texture sampling: %s", mode_name(cfg.mode)))
    return true
end)

local function draw_panel()
    TOOLS_UI.header("Texture Sampling")
    TOOLS_UI.status_badge(cfg.enabled)
    ui.same_line()
    ui.text("texturing experiment")

    if not sampling_capable() then
        ui.text_disabled(
            "Not available on this backend (Direct3D 8 renderer only); " ..
                "mode stays harmless when set.")
        return
    end

    TOOLS_UI.checkbox(
        "Enable Sampling", cfg, "enabled",
        "Override stage filters on textured 3D geometry"
    )
    if not cfg.enabled then
        return
    end

    TOOLS_UI.slider_int(
        "Sampling Mode", cfg, "mode", 0, MODE_COUNT - 1,
        "0 normal, 1 point, 2 linear, 3 anisotropic"
    )
    local mode = MODES[cfg.mode + 1]
    ui.text("Active:")
    ui.same_line()
    ui.text_colored(0.26, 0.59, 0.98, 1.0, mode.name)
    ui.tooltip(mode.desc)

    if ui.collapsing_header("All Modes", false) then
        for i, entry in ipairs(MODES) do
            if i == cfg.mode + 1 then
                ui.text_colored(0.26, 0.59, 0.98, 1.0, format("%d. %s", i, entry.name))
            else
                ui.text(format("%d. %s", i, entry.name))
            end
            ui.same_line()
            ui.text_disabled(format("- %s", entry.desc))
        end
    end

    ui.spacing()
    ui.text_disabled("Applies to textured triangles only; particles, lines " ..
                         "and HUD keep their own filter state.")
    ui.spacing()
    TOOLS_UI.keybind("F6", "Cycle sampling modes")
end

if _G.TOOLS_UI then
    TOOLS_UI.register_panel("textures", "Textures", draw_panel)
end

sdk.on_load(function()
    if type(cfg.mode) ~= "number" or cfg.mode < 0 or cfg.mode > MODE_COUNT - 1 then
        cfg.mode = DEFAULT.mode
        log_warn("Invalid texture sampling mode, using default")
    end
    log_info("Texture sampling plugin loaded (F6 cycles modes)")
end)

sdk.on_unload(function()
    if set_sampling then
        set_sampling(0)
    end
    published_mode = nil
    log_info("Texture sampling plugin unloaded")
end)

return M
