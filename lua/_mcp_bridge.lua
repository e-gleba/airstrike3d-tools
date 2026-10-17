---@meta
--- Internal MCP bridge pump.
---
--- @module _mcp_bridge
--- @version 1.0.0
---
--- Moves requests queued by the MCP transport (see
--- `src/proxy/sdk/mcp/readme.md` and `scripts/mcp/game_bridge.py`) onto the
--- render thread: the drain runs inside `sdk.mcp_poll` while the Lua stack
--- is up. Loads before the letter-prefixed plugins (`_` prefix) and does
--- nothing else.

if not sdk.mcp_poll then
    -- Proxy DLL built before the MCP transport existed; silently idle.
    return
end

sdk.on_frame(sdk.mcp_poll)
