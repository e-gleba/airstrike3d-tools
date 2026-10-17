# MCP transport (`sdk/mcp/`)

Lets an agent outside the game (OpenCode MCP via
`scripts/mcp/game_bridge.py`) inspect and drive the running game. The module
is deliberately isolated: it touches no existing proxy surfaces beyond the
read-only option getters for `STATUS`, and it never sees a `lua_State*`.

## Shape

- `mcp.hpp` — public seam: `start()` (loopback listener, idempotent) plus
  `take_request()` / `complete_request()` for the render-thread drain.
- `detail/mcp_transport_win32.cpp` — Winsock transport: listener thread
  (spawned detached, lives until process exit), one request per connection,
  length-delimited frame protocol:

  ```
  request  "<OP> <body-length>\n<body>"
  reply    "OK <body-length>\n<body>"  or  "ERR <body-length>\n<body>"
  ```

  Ops: `STATUS` (answered on the listener thread: exe, backend,
  `cheats_available()`, `fullscreen_4k`, uptime) and `RUN_LUA` (queued for
  the render thread, resolved with a 5 s deadline).

- `sdk/core/hooks.cpp` calls `start()` after plugin load (+1 line there).
- The scripting engine owns the Lua side: `engine_lua.cpp` registers
  `sdk.mcp_poll` and hands the drained chunk through `luaL_loadbuffer` +
  `lua_pcall` while the render thread is already inside a plugin frame —
  no new threads ever touch the Lua stack.
- `lua/_mcp_bridge.lua` pumps `sdk.mcp_poll()` every frame.

Thread-safety contract: the transport globals are `std::mutex`-guarded
plain strings and a `std::condition_variable` for the reply side; the
render-thread fast path is one relaxed atomic read.

## Networking

`127.0.0.1` only, default port **13373**, overridable through
`AS3D_MCP_PORT` (set the same variable for the bridge). Running two games
at once disables the transport in the second instance (port busy warning
in the SDK log).

## Host side

`scripts/mcp/game_bridge.py` (stdlib only) is the MCP stdio server registered
in `.opencode/opencode.jsonc` under `mcp.servers.as3d_bridge`:

- `game_status` — transport `STATUS`.
- `run_lua(code)` — runs in the plugin sandbox (`sdk.*` in scope); must
  finish within one frame; `return` a nil/number/string value to see it.
- `tail_game_log(lines)` — local read of the newest
  `<game-dir>/logs/sdk_*.log`; dir comes from `AS3D_MCP_GAME_DIR`.

Probe mode bypasses MCP framing for manual checks:

```bash
python3 scripts/mcp/game_bridge.py --probe game_status
python3 scripts/mcp/game_bridge.py --probe run_lua --code "return 1+1"
```

## Verification

- `proxy_rendering_state` (Proton unit binary) covers the option surfaces.
- Emulator launches log `mcp transport listening on 127.0.0.1:13373`.
- End-to-end drive is manual (game under Proton + `--probe` round trips),
  MCP method so single-line results cannot be scripted into ctest.
