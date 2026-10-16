---
description: Add a platform preset the right way (presets file, toolchain, validation)
agent: build
---

Add preset for `$ARGUMENTS` (required: compiler + platform + arch, e.g. `llvm-mingw windows x86_64`).

Follow `AGENTS.md` ## Session mechanics. Constraints from `AGENTS.md` (non-negotiable):

- Edit `cmake/presets/*.json` only. New file → also add it to the
  `CMakePresets.json` `include` list. Never hack platform logic into
  `CMakeLists.txt` or CI yaml.
- Naming: `<name>` configure, `<name>-build-release/debug` build,
  `<name>-test-release/debug` test, `<name>-package` CPack, `<name>-release/debug[-with-tests]` workflow. `binaryDir` stays
  `build/<preset>`.
- Every configure preset needs matching build presets (+ test if it runs under CTest).
  New presets do NOT change CI matrix rows: new rows reuse existing
  presets only. Say so explicitly instead of wiring CI.
- llvm-mingw variants inherit `llvm-mingw-base` pattern (toolchainFile + version/host_os/auto_download).

Steps:
1. If `$ARGUMENTS` lacks compiler, platform, or arch, ask through the
   native question tool — options grounded in `cmake/presets/*.json`
   siblings — then continue. Never guess the triple.
2. Read `cmake/presets/base.json` + the closest sibling preset file for the pattern.
3. Create configure + build (+ test, + package/workflow if shipping).
4. Cross only: reuse `cmake/toolchains/llvm_mingw.cmake`, keep
   `CMAKE_FIND_ROOT_PATH` reachable for `find_package` configs.
5. Validate: `cmake --list-presets` shows it, then `cmake --preset <new-preset>` configures clean.

Report: files touched, preset names created, configure OK/FAILED + first error line.
