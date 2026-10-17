---@meta
--- Fullscreen 4K experiment (Direct3D 8 only).
---
--- @module fullscreen_4k
--- @version 1.0.0
---
--- Toggles `visual_settings.fullscreen_4k`: the D3D8 hook rewrites the next
--- CreateDevice/Reset presentation parameters to fullscreen 3840x2160
--- (`width`, `height`, `windowed`). Mid-game toggles take effect on the next
--- device reset — e.g. Alt+Enter into fullscreen or a resolution change in
--- the game menu. The adapter must accept the requested backbuffer format
--- in fullscreen; otherwise the hook leaves the game's presentation
--- untouched and logs once.
---
--- The setting is an experiment: the engine may mis-scale screen-space UI it
--- was never authored for. Default stays off.
---
--- Hotkey: F7 — toggle fullscreen 4K

local M = {}

---@class Fullscreen4kConfig
---@field enabled boolean

local DEFAULT <const> = {
    enabled = false,
}

---@type Fullscreen4kConfig
local cfg = {
    enabled = DEFAULT.enabled,
}

local VK_TOGGLE = VK.F7
local format    = string.format
local log_info  = sdk.log_info
local log_warn  = sdk.log_warn

local warned_missing = false

sdk.on_key_down(
    function(vk)
        if vk ~= VK_TOGGLE then
            return false
        end
        if not sdk.set_fullscreen_4k then
            return true
        end
        cfg.enabled = not cfg.enabled
        sdk.set_fullscreen_4k(cfg.enabled)
        log_info(format("Fullscreen 4K: %s (applies at the next device "
                            .. "reset)",
                        cfg.enabled and "on" or "off"))
        if cfg.enabled and sdk.get_render_backend() == "opengl" then
            log_warn("Fullscreen 4K is a Direct3D 8 experiment; the OpenGL "
                         .. "path has no device reset hook")
        end
        return true
    end
)

sdk.on_load(
    function()
        if not sdk.set_fullscreen_4k then
            if not warned_missing then
                warned_missing = true
                log_warn("Fullscreen 4K needs a rebuilt bass.dll "
                             .. "(sdk.set_fullscreen_4k missing)")
            end
            return
        end
        log_info("Fullscreen 4K plugin loaded (F7 toggles, applies on the "
                     .. "next device reset)")
    end
)

sdk.on_unload(
    function()
        if cfg.enabled and sdk.set_fullscreen_4k then
            cfg.enabled = false
            sdk.set_fullscreen_4k(false)
        end
        log_info("Fullscreen 4K plugin unloaded")
    end
)

return M
