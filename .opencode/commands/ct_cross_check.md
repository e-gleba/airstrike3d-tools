---
description: Configure-check llvm-mingw cross preset without a full build
agent: build
---

Cross check for `$ARGUMENTS` (one of `llvm-mingw`, `clang-windows`, `msvc`, `all`).
If empty, check `llvm-mingw`.
Configure only. Never build Release unless asked.

Follow `AGENTS.md` ## Session mechanics. Follow `.agents/shared/ct_test_loop.md` for the
native-vs-cross split.

Preset map (verify with the listing below, do not guess variants):
- `llvm-mingw` → `llvm-mingw-i686` (`cmake/toolchains/llvm_mingw.cmake`, auto-downloaded toolchain)
- `clang-windows` → `clang_windows_x86` (Windows-only, `condition` gates it on Linux)
- `msvc` → `msvc` (Windows-only VS17)

```text
!`cmake --list-presets 2>/dev/null | grep -E "mingw|clang|msvc"`
```

Steps:
1. `cmake --preset <representative>` per requested platform.
2. Toolchain bootstrap (llvm-mingw download to `llvm_mingw/`) is expected on first run; only errors after bootstrap count as failures.
3. Never "fix" a cross failure by editing shared `cmake/presets/base.json`.

Report as a table: preset | OK/FAILED | first error line (failures only).
Emulator tests need Steam + Proton — mention, don't run, unless asked.
