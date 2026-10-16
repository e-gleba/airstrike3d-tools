#!/usr/bin/env python3
"""House-rules hook: refuse banned CMake/C++ patterns.

Modes: --staged (pre-commit), --file PATH (opencode formatter entry),
stdin JSON (Claude-style edit hook). Exit 0 = clean, else violations.

Usage as a Claude-style edit hook (reads JSON payload from stdin),
as an opencode formatter entry (takes --file PATH),
or as a pre-commit check: python3 .agents/scripts/rules/hook.py --staged

Pattern from telegramdesktop/tdesktop@dev, tools/rules/hook.py
(https://github.com/telegramdesktop/tdesktop/blob/dev/tools/rules/hook.py):
edit-hook stdin-JSON + --staged dual mode. All checks below are original
to this repo (CMake/C++ bans per AGENTS.md + .opencode/REVIEW.md); no lines copied.
Same license as this repo (MIT, see license.md).
"""

from __future__ import annotations

import json
import re
import subprocess
import sys
from pathlib import Path

Ban = tuple[str, str]

CMAKE_BANNED: list[Ban] = [
    (r"\binclude_directories\s*\(", "use target_include_directories()"),
    (r"\blink_libraries\s*\(", "use target_link_libraries()"),
    (r"\badd_definitions\s*\(", "use target_compile_definitions()"),
    (r"\bfile\s*\(\s*GLOB\b", "list sources explicitly, no file(GLOB)"),
    (r"CMAKE_CXX_FLAGS(?!_)", "use target_compile_options(), never CMAKE_CXX_FLAGS"),
    (r"CMAKE_BUILD_TYPE", "preset owns CMAKE_BUILD_TYPE, not CMakeLists"),
    (
        r"\b(?:set|option|function|macro|foreach)\s*\(\s*_[A-Za-z0-9_]*",
        "no _-prefixed names, use AS3D_/as3d_/lowercase (_ is upstream-reserved)",
    ),
]

CPP_BANNED: list[Ban] = [
    (r"\bnew\s+\w", "RAII only, no new/delete"),
    (r"\bdelete\b", "RAII only, no new/delete"),
    (r"static_cast\s*<\s*void\s*>", "never discard [[nodiscard]] with a cast"),
    (r"\(void\)", "never discard [[nodiscard]] with a cast"),
    (r"using\s+namespace\b", "never add file-scope using namespace"),
]

CMAKE_SUFFIXES = (".cmake", "CMakeLists.txt")
CPP_SUFFIXES = (".cpp", ".h", ".hpp", ".cppm", ".cxx")

RULES_HINT = (
    "Rules live in AGENTS.md + .opencode/REVIEW.md. Fix before anything else."
)


def is_cmake(path: str) -> bool:
    """Check whether path is a CMake file."""
    return path.endswith(CMAKE_SUFFIXES)


def is_cpp(path: str) -> bool:
    """Check whether path is a C++ file."""
    return path.endswith(CPP_SUFFIXES)


def check_text(text: str, cmake: bool) -> list[str]:
    """Return one error per banned pattern found in text."""
    errors: list[str] = []
    for pattern, fix in CMAKE_BANNED if cmake else CPP_BANNED:
        if re.search(pattern, text):
            errors.append(f"banned `{pattern}` -- {fix}")
    return errors


def written_text(payload: dict[str, object]) -> str:
    """Join every written chunk from an edit-hook payload."""
    tool = payload.get("tool_input")
    if not isinstance(tool, dict):
        return ""
    parts = [tool.get("new_string") or "", tool.get("content") or ""]
    edits = tool.get("edits")
    if isinstance(edits, list):
        parts += [e.get("new_string") or "" for e in edits if isinstance(e, dict)]
    return "\n".join(parts)


def payload_path(payload: dict[str, object]) -> str:
    """Prefer the edited file path, fall back to the requested one."""
    tool = payload.get("tool_input")
    tool_path = tool.get("file_path", "") if isinstance(tool, dict) else ""
    resp = payload.get("tool_response")
    resp_path = resp.get("filePath", "") if isinstance(resp, dict) else ""
    path = resp_path or tool_path
    return path if isinstance(path, str) else ""


def staged() -> int:
    """Check added lines of staged CMake/C++ files. Returns exit code."""
    diff = subprocess.run(
        ["git", "diff", "--cached", "-U0", "-M"],
        capture_output=True,
        text=True,
        check=False,
    ).stdout
    path, bad = "", 0
    for line in diff.split("\n"):
        if line.startswith("+++ b/"):
            path = line[6:]
        elif (
            line.startswith("+")
            and not line.startswith("+++")
            and (is_cmake(path) or is_cpp(path))
        ):
            for err in check_text(line[1:], is_cmake(path)):
                print(f"{path}: {err}", file=sys.stderr)
                bad += 1
    return 1 if bad else 0


def edit_hook() -> int:
    """Check an edit-hook JSON payload from stdin. Returns exit code."""
    try:
        payload = json.load(sys.stdin)
    except (ValueError, OSError):
        return 0
    if not isinstance(payload, dict):
        return 0
    path = payload_path(payload)
    if not (is_cmake(path) or is_cpp(path)):
        return 0
    errors = check_text(written_text(payload), is_cmake(path))
    for err in errors:
        print(f"{Path(path).name}: {err}", file=sys.stderr)
    if errors:
        print(RULES_HINT, file=sys.stderr)
        return 2
    return 0


def file_mode(path: str) -> int:
    """Check one file's content (opencode formatter entry). Returns exit code."""
    try:
        text = Path(path).read_text(encoding="utf-8")
    except OSError as exc:
        print(f"{path}: cannot read ({exc})", file=sys.stderr)
        return 1
    if not (is_cmake(path) or is_cpp(path)):
        return 0
    errors = check_text(text, is_cmake(path))
    for err in errors:
        print(f"{Path(path).name}: {err}", file=sys.stderr)
    return 1 if errors else 0


def main() -> int:
    """Dispatch: --staged for pre-commit, --file for formatters, else edit-hook."""
    if "--staged" in sys.argv:
        return staged()
    if "--file" in sys.argv:
        idx = sys.argv.index("--file")
        if idx + 1 >= len(sys.argv):
            print("house-rules: --file needs a path", file=sys.stderr)
            return 1
        return file_mode(sys.argv[idx + 1])
    return edit_hook()


if __name__ == "__main__":
    sys.exit(main())
