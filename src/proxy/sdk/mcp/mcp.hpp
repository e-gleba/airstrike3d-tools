/// @file mcp.hpp
/// @brief Loopback MCP transport (in-game side).
///
/// Owns the listener thread that serves `scripts/mcp/game_bridge.py`
/// (registered in `.opencode/opencode.jsonc`) over 127.0.0.1. Lua-bound
/// objects never live in this module: `run_lua` requests are queued here and
/// drained on the render thread by the scripting engine's `sdk.mcp_poll`
/// binding. See `sdk/mcp/readme.md` for the whole moving set.

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace sdk::mcp
{

/// @brief Starts the loopback transport server (idempotent).
///
/// Configures Winsock, opens `127.0.0.1:<port>` (default 13373, override via
/// the `AS3D_MCP_PORT` environment variable) and spawns the detached listener
/// thread. The thread is never joined: it exists for the process lifetime.
void start() noexcept;

/// @brief Atomically takes one queued `run_lua` request.
/// @param out_id Receives the request id to hand to `complete_request()`.
/// @return The Lua source text; empty when no request is queued.
///
/// Runs on the render thread (called once per frame from the plugin
/// `_mcp_bridge.lua` through the `sdk.mcp_poll` binding).
[[nodiscard]] std::string take_request(std::uint64_t& out_id) noexcept;

/// @brief Publishes the outcome of a request taken earlier.
/// @param id      The id returned by `take_request()`.
/// @param outcome Protocol text: `ok/...` on success, `error/...` on failure.
void complete_request(std::uint64_t id, std::string_view outcome) noexcept;

} // namespace sdk::mcp
