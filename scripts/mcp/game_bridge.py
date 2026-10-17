#!/usr/bin/env python3
"""stdio MCP bridge to the bass.dll proxy in-game MCP transport.

Moving parts (see ``src/proxy/sdk/mcp/readme.md`` for the C++ side):

- The proxy SDK inside the running game listens on ``127.0.0.1:<port>`` and
  speaks a tiny length-delimited frame protocol (one request per connection).
- This script is the stdio MCP server (newline-delimited JSON-RPC 2.0) that
  OpenCode spawns; it forwards tool calls to the transport.

Tools:

- ``game_status`` — exe, backend, cheats, uptime (transport ``STATUS`` op).
- ``run_lua`` — run a Lua snippet in the game's render-thread plugin sandbox
  (transport ``RUN_LUA`` op) and return its ``return`` value.
- ``tail_game_log`` — read the tail of the newest ``logs/sdk_*.log`` under
  the game deploy directory (local file read; no game round-trip).

Environment:

- ``AS3D_MCP_PORT`` — transport port (default ``13373``; must match the
  game's ``AS3D_MCP_PORT`` when it is set).
- ``AS3D_MCP_GAME_DIR`` — deploy directory for ``tail_game_log`` (relative
  paths resolve from this script's working directory).

Probe mode for manual checks (no MCP framing):

    python3 scripts/mcp/game_bridge.py --probe game_status
    python3 scripts/mcp/game_bridge.py --probe run_lua --code "return 1+1"
"""

from __future__ import annotations

import argparse
import json
import os
import socket
import sys
from collections import deque
from pathlib import Path

PROTOCOL_VERSION = "2024-11-05"
SERVER_INFO = {"name": "as3d_game_bridge", "version": "1.0.0"}
SERVER_INSTRUCTIONS = (
    "Bridge to the running AirStrike 3D game (bass.dll SDK). Tools fail "
    "cleanly when the game is not running. run_lua executes inside the "
    "plugin sandbox on the render thread: snippets must finish within one "
    "frame (never busy-loop or sleep) and should end with a return "
    "statement; only nil, number and string return values are serialized. "
    "Cheat inputs go through sdk.cheat_trigger(code); visual experiments "
    "through sdk.set_visual_mode / sdk.set_texture_sampling / "
    "sdk.set_fullscreen_4k. game_status reports a frame heartbeat "
    "(frames_age_seconds = seconds since the last BeginScene; -1 before "
    "the first frame), so a stalled render loop still answers with a "
    "growing age."
)
DEFAULT_PORT = "13373"
DEFAULT_LOG_LINES = 60
K_CONNECT_TIMEOUT = 5.0
K_EXECUTE_TIMEOUT = 15.0
K_HEADER_MAX = 64
K_REPLY_MAX = 8 * 1024 * 1024


class ToolError(Exception):
    """Tool failure that maps to an MCP ``isError`` result."""


def transport_address() -> tuple[str, int]:
    """Return the transport host/port from the environment."""
    raw = os.environ.get("AS3D_MCP_PORT", DEFAULT_PORT)
    try:
        port = int(raw)
    except ValueError:
        print(
            f"bridge: invalid AS3D_MCP_PORT {raw!r}, using {DEFAULT_PORT}",
            file=sys.stderr,
        )
        port = int(DEFAULT_PORT)
    return "127.0.0.1", port


def _read_frame(sock: socket.socket) -> tuple[bool, str]:
    """Read one full ``<STATE> <length>\\n<body>`` reply frame.

    TCP may coalesce the header with part of the body in the first segment,
    so the newline split preserves the tail instead of losing it.
    """
    buffer = b""
    while buffer.find(b"\n") < 0:
        chunk = sock.recv(4096)
        if not chunk:
            raise OSError("connection closed before a reply")
        buffer += chunk
        if buffer.find(b"\n") < 0 and len(buffer) > K_HEADER_MAX:
            raise OSError("reply header is too large")
    header, _, prefix = buffer.partition(b"\n")
    state_text, _, length_text = header.decode("ascii").partition(" ")
    length = int(length_text)
    if length > K_REPLY_MAX:
        raise OSError(f"reply body is too large: {length}")
    body = bytearray(prefix)
    while len(body) < length:
        chunk = sock.recv(min(64 * 1024, length - len(body)))
        if not chunk:
            raise OSError("connection closed mid-reply")
        body.extend(chunk)
    is_ok = state_text == "OK"
    return is_ok, bytes(body[:length]).decode("utf-8", errors="replace")


def exchange(op: str, payload: bytes = b"") -> tuple[bool, str]:
    """Run one request against the in-game transport over loopback."""
    host, port = transport_address()
    try:
        with socket.create_connection((host, port), timeout=K_CONNECT_TIMEOUT) as sock:
            sock.settimeout(K_EXECUTE_TIMEOUT)
            sock.sendall(f"{op} {len(payload)}\n".encode("ascii") + payload)
            return _read_frame(sock)
    except (OSError, socket.timeout) as error:
        raise ToolError(
            f"game transport unreachable at {host}:{port} ({error}); "
            "is the game running?"
        ) from error


def tool_game_status() -> str:
    """Read-only game status line bundle from the transport STATUS op."""
    ok, text = exchange("STATUS")
    if not ok:
        raise ToolError(text or "status request failed")
    return text


def tool_run_lua(code: str) -> str:
    """Run a Lua snippet on the render thread (transport RUN_LUA op)."""
    if not code or not code.strip():
        raise ToolError("run_lua needs a non-empty Lua source string")
    ok, text = exchange("RUN_LUA", code.encode("utf-8"))
    if not ok:
        # The outcome carries "error:<lua message>" from the pump or a
        # timeout note from the transport itself.
        raise ToolError(text)
    if text == "ok":
        return "ok (no return value)"
    return text.removeprefix("ok:")


def newest_sdk_log(game_dir: str) -> Path:
    """Find the newest ``sdk_*.log`` under the deploy directory."""
    logs = sorted((Path(game_dir) / "logs").glob("sdk_*.log"))
    if not logs:
        raise ToolError(f"no sdk_*.log under {game_dir}/logs")
    return max(logs, key=lambda path: path.stat().st_mtime)


def tool_tail_game_log(lines: int) -> str:
    """Tail the newest game SDK log without touching the game."""
    game_dir = os.environ.get("AS3D_MCP_GAME_DIR")
    if not game_dir:
        raise ToolError("AS3D_MCP_GAME_DIR is not set")
    log_file = newest_sdk_log(game_dir)
    with log_file.open("r", encoding="utf-8", errors="replace") as handle:
        tail = deque(handle, maxlen=lines)
    if not tail:
        return f"{log_file}: empty"
    return "".join(tail).rstrip()


TOOLS = [
    {
        "name": "game_status",
        "title": "Game status",
        "description": "Read-only status from the running game: exe name, "
        "render backend, cheat dispatcher availability, fullscreen_4k "
        "flag and uptime.",
        "inputSchema": {"type": "object", "properties": {}},
    },
    {
        "name": "run_lua",
        "title": "Run Lua in the game",
        "description": "Runs a Lua snippet inside the game's render thread "
        "plugin sandbox (sdk.* bindings in scope). Must finish within one "
        "frame: never busy-loop or sleep. End with a return statement; "
        "only nil, number and string return values are serialized.",
        "inputSchema": {
            "type": "object",
            "properties": {"code": {"type": "string", "description": "Lua source"}},
            "required": ["code"],
            "additionalProperties": False,
        },
    },
    {
        "name": "tail_game_log",
        "title": "Tail the game SDK log",
        "description": "Returns the tail of the newest logs/sdk_*.log in "
        "AS3D_MCP_GAME_DIR (the deploy directory). Local file read; works "
        "while the game runs or after it exited.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "lines": {
                    "type": "integer",
                    "description": "Number of trailing lines (default 60)",
                    "default": DEFAULT_LOG_LINES,
                    "minimum": 1,
                }
            },
            "additionalProperties": False,
        },
    },
]


def call_tool(name: str, arguments: dict[str, object]) -> str:
    """Dispatch one tools/call and return the text payload."""
    if name == "game_status":
        return tool_game_status()
    if name == "run_lua":
        code = arguments.get("code")
        if not isinstance(code, str):
            raise ToolError(f"run_lua: expected a string code, got {type(code)}")
        return tool_run_lua(code)
    if name == "tail_game_log":
        raw = arguments.get("lines", DEFAULT_LOG_LINES)
        if isinstance(raw, bool) or not isinstance(raw, int):
            raise ToolError("tail_game_log: lines must be an integer")
        return tool_tail_game_log(max(1, raw))
    raise ToolError(f"unknown tool: {name}")


def send_message(message: dict[str, object]) -> None:
    """Write one newline-delimited JSON-RPC message to stdout."""
    sys.stdout.write(json.dumps(message, separators=(",", ":")) + "\n")
    sys.stdout.flush()


def tool_result(request_id: object, text: str, *, is_error: bool) -> None:
    """Emit a tools/call result with a single text content entry."""
    send_message(
        {
            "jsonrpc": "2.0",
            "id": request_id,
            "result": {
                "content": [{"type": "text", "text": text}],
                "isError": is_error,
            },
        }
    )


def handle_call(request: dict[str, object]) -> None:
    """Handle a single JSON-RPC request/notification line."""
    method = request.get("method")
    request_id: object = request.get("id")
    if method == "initialize":
        params = request.get("params", {}) or {}
        requested = params.get("protocolVersion") or PROTOCOL_VERSION
        if not isinstance(requested, str):
            requested = PROTOCOL_VERSION
        send_message(
            {
                "jsonrpc": "2.0",
                "id": request_id,
                "result": {
                    "protocolVersion": requested,
                    "capabilities": {"tools": {"listChanged": False}},
                    "serverInfo": SERVER_INFO,
                    "instructions": SERVER_INSTRUCTIONS,
                },
            }
        )
        return
    if method == "notifications/initialized":
        return
    if method == "tools/list":
        send_message({"jsonrpc": "2.0", "id": request_id, "result": {"tools": TOOLS}})
        return
    if method == "tools/call":
        params = request.get("params", {}) or {}
        try:
            name = str(params.get("name", ""))
            arguments = params.get("arguments", {}) or {}
            if not isinstance(arguments, dict):
                raise ToolError("tool arguments must be an object")
            tool_result(request_id, call_tool(name, arguments), is_error=False)
        except ToolError as error:
            tool_result(request_id, str(error), is_error=True)
        except (OSError, ValueError, json.JSONDecodeError) as error:
            tool_result(request_id, f"bridge failure: {error!r}", is_error=True)
        return
    if method == "ping":
        send_message({"jsonrpc": "2.0", "id": request_id, "result": {}})
        return
    if isinstance(method, str) and method.startswith("notifications/"):
        return
    send_message(
        {
            "jsonrpc": "2.0",
            "id": request_id,
            "error": {"code": -32601, "message": f"{method!r} not supported"},
        }
    )


def run_loop() -> None:
    """Serve the stdio MCP loop until stdin closes."""
    for raw_line in sys.stdin:
        line = raw_line.strip()
        if not line:
            continue
        try:
            request = json.loads(line)
        except json.JSONDecodeError as error:
            send_message(
                {
                    "jsonrpc": "2.0",
                    "id": None,
                    "error": {
                        "code": -32700,
                        "message": f"bad JSON line: {error}",
                    },
                }
            )
            continue
        if isinstance(request, dict):
            handle_call(request)


def probe(argv: list[str]) -> int:
    """Run one tool call directly against the transport and print it."""
    tool_names = [tool["name"] for tool in TOOLS]
    parser = argparse.ArgumentParser(
        prog="game_bridge.py --probe",
        description="Run one MCP tool call against the running game.",
    )
    parser.add_argument("tool", choices=sorted(tool_names))
    parser.add_argument("--code", help="Lua source for run_lua")
    parser.add_argument(
        "--lines",
        type=int,
        default=DEFAULT_LOG_LINES,
        help=f"log tail length for tail_game_log (default {DEFAULT_LOG_LINES})",
    )
    args = parser.parse_args(argv)
    arguments: dict[str, object] = {}
    if args.tool == "run_lua":
        arguments["code"] = args.code or ""
    if args.tool == "tail_game_log":
        arguments["lines"] = args.lines
    try:
        text = call_tool(str(args.tool), arguments)
    except ToolError as error:
        print(f"ERR: {error}", file=sys.stderr)
        return 1
    sys.stdout.write(text if text.endswith("\n") else text + "\n")
    return 0


def main() -> int:
    """Entry point: probe mode or the stdio MCP loop."""
    argv = sys.argv[1:]
    if argv and argv[0] == "--probe":
        return probe(argv[1:])
    run_loop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
