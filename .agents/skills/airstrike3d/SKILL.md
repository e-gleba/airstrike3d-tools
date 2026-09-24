---
name: airstrike3d
description: Project workflow skill for the airstrike3d-tools repo (AirStrike 3D reverse-engineering toolkit). Use when building, testing, editing game deployments (src/2_06, src/2_51, src/2_71, src/space_strike), the BASS proxy DLL, or binary parsers. Activates on queries about building, presets, CTest, proxy DLL, deploy, game versions, or repo conventions.
---

# AirStrike3D Tools — Project Skill

## Role

Senior C++ systems engineer for reverse-engineering tooling. Priorities in
order: safety first, correctness, maintainability, performance last (only
when measured).

## Layout

- `src/2_06/`, `src/2_51/`, `src/2_71/`, `src/space_strike/` — game
  deployments (exe, `bass.dll`, `data/`, `config.ini.in`). Deploy output lands
  in `build/<preset>/src/<version>/`.
- `src/proxy/` — BASS proxy DLL SDK + version shims.
- `src/decompiled_game/` — decompiled game logic.
- `cmake/` — `deploy_game.cmake`, `proton_testing.cmake`, toolchains.
- `tests/proxy/` — unit tests.
- `ghidra/` — **read-only reference material. Never modify, never index.**

## Build & test (Linux host → Win32)

```bash
cmake --preset llvm-mingw-i686                      # configure
cmake --workflow --preset llvm-mingw-i686-debug     # configure + Debug build (validate here)
cmake --workflow --preset llvm-mingw-i686-release   # configure + Release build + package
ctest --preset llvm-mingw-i686-test-debug -L deploy # fast deploy-tier tests, no Proton needed
```

Proton/launch CTest tiers require Steam + Proton — skip them when absent,
never force them.

## Rules

- Modern C++23: RAII, `std::span` for buffers, `[[nodiscard]]`,
  `noexcept` where applicable. No raw `new`/`delete`, no C-style casts,
  initialize all variables.
- Treat all binary input as untrusted: bounds-check every read.
- Game deployments stay declarative: data + a `CMakeLists.txt` calling
  `add_game_deployment()`, no logic.
- Tests required for new behavior; `ctest` must be green before commit.
- Ask first before: new dependencies, parser changes, proxy export changes,
  CMake system changes, disabling tests.
- Never: modify `ghidra/`; raw `new`/`delete`; C-style casts; disabled
  compiler warnings; `#define` constants; hardcoded paths; committing
  without green tests.

## Definition of done

Fresh `llvm-mingw-i686-debug` workflow passes and `ctest -L deploy` is green
before pushing. No C++ change without a build; no behavior change without
test evidence.
