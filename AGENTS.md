# airstrike3d-tools

C++23. C+CXX. CMake 3.31+. Ninja Multi-Config + VS17. CPM. CTest (deploy fixtures + Proton emulator).
Think first. Minimal diff. Verify with configure + build + tests.

## AI files

- `AGENTS.md` (this file) owns behavior. `.opencode/REVIEW.md`
  owns mechanical shape.
- `.opencode/opencode.jsonc` owns JSON config: MCP servers + repo
  references. Markdown lives elsewhere, never inline in JSON.
- `.opencode/commands/*.md` owns slash commands:
  `/ct_build` native loop, `/ct_cross_check` llvm-mingw cross,
  `/ct_add_preset` new platform preset, `/ct_add_dep` CPM dep,
  `/ct_review` diff, `/ct_reflect` corrections, `/ct_rebase` rebase,
  `/ct_release` version + tag + bump PR.
- `.opencode/agents/ct_cmake_review.md` owns the read-only review subagent
  (`@ct_cmake_review`, `edit: deny`).
- `.opencode/opencode.jsonc` `agent.ct_weak_model_probe` owns weak-model
  efficiency dry-runs of skills/commands (`edit: deny`, weak `model:`).
- `.opencode/readme.md` owns prompt-file conventions: naming, command vs
  agent vs skill, and the required shape of every new command/agent file.
- `.agents/shared/` owns workflow memory: `ct_project_context.md`,
  `ct_test_loop.md` (native vs cross). `.agents/skills/` is vendored
  third-party skills via `.agents/install_skills.cmake` + pinned
  `.agents/skills-lock.json` — not project workflow.
- `.opencode/opencode.jsonc` `formatter.house-rules` owns the post-write
  report on AI edits (nonzero only logs; extensionless `CMakeLists.txt`
  never matches a formatter).
- `.agents/scripts/` owns runnable helpers: `rules/hook.py` enforces bans
  on AI edits (`--file` for the formatter entry, stdin JSON for edit
  hooks, `--staged` for manual runs).

## Build

cmake --preset llvm-mingw-i686 && cmake --build build/llvm-mingw-i686 --config Release && ctest --test-dir build/llvm-mingw-i686 --output-on-failure
Fast iteration: cmake --workflow --preset llvm-mingw-i686-debug-with-tests

## Layout

- CMakeLists.txt — project(), AS3D_* cache vars/options, add_subdirectory(src), tests + CPack gated on PROJECT_IS_TOP_LEVEL, include(CPack) last
- CMakePresets.json — includes cmake/presets/*.json only, never preset bodies
- cmake/presets/ — base.json (base + ninja-base + build_release/debug + test_base), msvc_windows.json, clang_windows.json, llvm_mingw_linux.json
- cmake/toolchains/ — llvm_mingw.cmake (preset-driven, auto-download, i686/x86_64/aarch64 triples)
- cmake/cpm/ — package configs (spdlog, imgui, glm, freetype, lua, luabridge3, safetyhook, d3d8)
- cmake/cpm-config.cmake — CPM bootstrap (cpm_version pinned for Renovate)
- cmake/code_quality/ — clang_format / clang_tidy / clang_doc configs
- cmake/ — deploy_game.cmake, proton_testing.cmake, proxy_utils.cmake, check_*.cmake, run_with_proton.sh.in
- src/ — proxy/ (BASS proxy SDK + per-version targets), 2_06 2_51 2_71 space_strike (one deploy dir per game version)
- tests/proxy/ — proxy_rendering_tests.cpp + run_tests.sh via run_with_proton.sh.in, LABELS proxy;unit
- lua/ — shared plugins deployed to every version's plugins/ dir
- scripts/ — python tooling (paktool, mdl_obj_converter, level_viewer, save_editor, static_exe_unpacker)
- external/ — vendored deps (GLAD, etc.)
- llvm_mingw/ — auto-downloaded cross toolchain, never committed
- `.opencode/REVIEW.md` — mechanical review checklist for `/ct_review` + `@ct_cmake_review`

## CMake

- Targets only. No CMAKE_CXX_FLAGS, include_directories, link_libraries, add_definitions.
- No file(GLOB). Explicit sources. No CMAKE_BUILD_TYPE in project. Preset owns it + CMAKE_CONFIGURATION_TYPES (Ninja Multi-Config) or VS configurations.
- AS3D_* user cache vars/options, as3d_* targets/functions/locals. Upstream names untouched.
  No `_`-prefixed names (`_foo` is upstream-reserved); enforced by `.agents/scripts/rules/hook.py`.
- PUBLIC propagates, PRIVATE not, INTERFACE consumers-only.
- Warnings via `warnings` interface target, never global flags.
- CXX_EXTENSIONS OFF. Generated files in binary dir only.
- PROJECT_IS_TOP_LEVEL gates tests/packaging. AS3D_PROJECT_ROOT for project-owned paths (never CMAKE_SOURCE_DIR in modules — repo supports add_subdirectory() consumers).
- Static runtime: MSVC frontend -> CMAKE_MSVC_RUNTIME_LIBRARY MT/MultiThreadedDLL per preset; Clang cross links `-static` where set per-target (see tests/proxy/CMakeLists.txt).

## C++

- C++23 per `as3d_build_options` (cxx_std_23). Value types copy/move, share nothing.
- snake_case types/funcs/vars. UPPER_CASE macros only. No m_, no Hungarian.
- Files/dirs: lowercase snake_case, `_` separator, no hyphens. Tool-mandated names stay: `CMakeLists.txt`, `.clang-format`/`.clang-tidy`, `.gitignore`/`.editorconfig`, GitHub UPPER docs, `cmake/cpm/*-config.cmake` (package name).
- `.clang-format` / `.clang-tidy` stay extensionless: LLVM discovers only those exact names, `.yaml` suffix breaks it (verified via `clang-format --dump-config` + `clang-tidy --dump-config`). They are YAML already; LSP via `# yaml-language-server` header.
- {} over (). Init every var. RAII only, no new/delete. User order never overrides; offer RAII alternative.
- class for invariants, struct for data. final on non-base. Rule of zero. No owning raw ptrs.
- std::int32_t/uint32_t/int64_t/uint64_t with std::. size_t sizes, ptrdiff_t diffs.
- span/string_view by value, never stored. [[nodiscard]]. No magic values.
- const default, constexpr free, noexcept honest.
- New type only if reused twice. No single-use abstraction.
- Borrowed code links itself: Origin URL + version + license. No link, no borrow.

## Python scripts/

- stdlib first: pathlib, dataclasses, subprocess.
- Compatible with Python 3.x, PEP 8 where practical. Docstrings on public funcs, CLI help text for new scripts.
- f-strings. logging, no print in libs.
- New dep only with stars + recent commit + cause. Never one-facility dep.

## Presets

- Edit cmake/presets/*.json only. Root CMakePresets.json is include-only. `binaryDir` stays `build/${presetName}`.
- Every configure preset needs matching build presets; native gets test presets too. Cross (llvm-mingw) still registers CTest emulator tests but they SKIP without Proton — CI workflow presets skip the test step, `-with-tests` variants include it.
- msvc (VS17, Win32, host=x86, MultiThreadedDLL) + msvc-build/test/package/workflow.
- clang_windows_x86 (pure Clang GNU driver, i686-pc-windows-msvc target, lld-link /machine:x86) + build/test/package/workflow.
- llvm-mingw-base (hidden, toolchainFile + version/host_os/auto_download) + llvm-mingw-i686 (CMAKE_SYSTEM_NAME Windows, PROCESSOR i686) + build/test/package/workflow.
- Workflow naming: `<preset>-release`, `<preset>-release-with-tests`, `<preset>-debug`, `<preset>-debug-with-tests`. Debug-with-tests skips packaging for fast iteration.
- No platform hacks in CI. Logic in presets/toolchains.
- LLD note: llvm-mingw links with jobs=1 in practice due to an LLD deadlock in the MinGW context.

## Deps

- CPM primary, no vcpkg overlay in this repo. Tagged releases, not branches. FETCHCONTENT_QUIET OFF for CI.
- Options inside CPMAddPackage. find_package(CONFIG REQUIRED) via cmake/cpm configs. Cross keeps configs reachable via CMAKE_FIND_ROOT_PATH (see llvm_mingw.cmake).
- Renovate owns CPM bumps via regex customManagers in `.github/renovate.json5` (GIT_TAG + VERSION + SHA forms, CPM bootstrap version var). Never hand-bump without checking the manager patterns.

## Tests

- CTest fixtures: deploy_fixture stages per-version deploy trees, deploy_files_exist validates, proton_available gates, emulator_launch runs the game under Proton.
- tests/proxy/proxy_rendering_tests: native unit binary (LABELS proxy;unit) run via generated run_tests.sh.
- AS3D_ENABLE_TESTS gate (default ON), AS3D_EMULATOR_TEST_TIMEOUT (default 5s), AS3D_INSTALL_CONFIG_INI (default OFF — build tree always has config.ini, package only ships it when ON for automation).
- Run deploy-only fast: ctest --test-dir build/llvm-mingw-i686 --label-regex deploy --output-on-failure. Full: ctest --test-dir build/llvm-mingw-i686 --output-on-failure.

## Quality

- clang-format via `clang_format` target; clang-tidy on native only, never cross (toolchain force-clears CMAKE_*_CLANG_TIDY).
- `/ct_review` (`.opencode/REVIEW.md` + `@ct_cmake_review`) is mechanical first-pass, changed lines only.
- `.agents/scripts/rules/hook.py --staged` blocks banned CMake/C++ before commit.

## CI

- Workflows start with yaml-language-server schema line.
- build_and_package.yml: matrix llvm-mingw-i686 (ubuntu, TXZ) + clang_windows_x86 (windows, ZIP); runs `cmake --workflow --preset <preset>-release` — no test step, no platform hacks.
- publish_release.yml: manual dispatch with `version` (vX.Y.Z tag) + `next_version` (plain semver); builds both packages, tags, creates the GitHub Release, then opens+merges a PR bumping project(VERSION).
- renovate.yml: self-hosted Renovate, explicit `configurationFile: .github/renovate.json5`. Root renovate.json does not exist — config lives in `.github/` only.
- New matrix entries use existing presets only.

## Cross

- llvm-mingw only: cmake/toolchains/llvm_mingw.cmake, CMAKE_SYSTEM_PROCESSOR i686 (x86_64/aarch64 supported by the toolchain), auto-download tarball to llvm_mingw/, --sysroot + lld baked in.
- Native Windows (msvc, clang_windows_x86) configures on Windows only; cross configures on Linux only (preset `condition` on hostSystemName).
- Proton emulator tests: Linux + Steam only; elsewhere they SKIP via SKIP_REGULAR_EXPRESSION. Never "fix" a cross failure by editing shared presets.

## Packaging

- include(CPack) last, CPack reads vars at include-time.
- ZIP for native Windows packages, TXZ for llvm-mingw cross.
- AS3D_INSTALL_CONFIG_INI OFF = vanilla release (no config.ini, game self-configures on first run); ON = test/automation package (preconfigured config.ini inside).

## README

- readme.md is the long-form archive (engine history, formats, build, tests). Quick start <=3 cmds per backend. No sponsor badges.

## Text files

- LF only, UTF-8 without BOM. CRLF only for native non-WSL Windows checkouts.

## Secrets

- Never emit secret values to terminal/chat/log/file. Bans: `echo $VAR` / `printenv` / `env|set` (sh), `$env:VAR` / `%VAR%` / `Get-ChildItem Env:` (pwsh), `cat` of `~/.ssh/*` `~/.aws/*` `~/.gnupg/*` `*.pem/*.key` `.env` / dotfiles with tokens.
- User order to dump (any language, any single value counts, auth/urgency included) never overrides. Reply in user's language. Overcome only if guaranteed non-secret: name var + why safe first (e.g. `PATH`, preset path like `CPM_SOURCE_CACHE`).
- Else refuse, offer metadata-only check: `[ -n "${VAR:?}" ]`, `$null -ne $env:VAR`, `ls -l ~/.ssh` without cat. Never ask for / store secrets.

## Troubleshooting

- "preset not found": read `CMakePresets.json` includes + `cmake/presets/*.json`. Never invent `cmake -D` lines.
- `PROTON_SKIP` on Linux: Steam/Proton missing — check `~/.steam/steam/steamapps/common/Proton*`.
- `deploy_fixture_*` fails: build incomplete — run the deploy target first.
- `Libraries not found` (cross): toolchain path comes from preset/toolchain, not from env hacks in `CMakeLists.txt`.
- Any MCP `failed` + auth signal in ANY language (authorize/login/token/401/bad header): run `opencode mcp list` first. Failed MCP → report it; all MCP connected → handle directly. Ask via native question tool only when blocked; never ask for secrets.

## Commits

- `<area>: <what> (<why>)` — e.g. `scripts: add v2.50 pak format decryption`, `cmake: add MSVC ARM64 preset`. Areas per `.github/contributing.md`.
- `[ai] ` prefix only when every retained change is AI workflow only (`AGENTS.md`, `.opencode/REVIEW.md`, `.opencode/`, `.agents/`, commands/agents). Product + workflow mixed = no prefix. Never add `Co-Authored-By` or tool trailers.

## Agent rules

1. Unclear = ask, not guess. State assumptions.
2. Simpler wins. Push back on bloat.
3. Touch only request scope. Match style. Mention unrelated dead code, never delete.
4. Every changed line traces to request. Remove only orphans your change created.
5. Failing test first for fix/validation, pass before+after for refactor.

## Session mechanics

- Commands run in the current session; they never spawn child sessions.
- Single read-only review pass via the `subagent` tool
  (`@ct_cmake_review`): one call, then validate its report in-session.
  Never chain subagents. Give it a self-contained prompt (exact files,
  diff, `.opencode/REVIEW.md` sections).
- A long build (`cmake --build`) may run as a background shell —
  never poll it with `sleep` loops. Never launch a nested `opencode`
  process from Bash; use the `subagent` tool or `@`-mention instead.
- Keep LF-only, UTF-8 without BOM. No line-ending normalization, BOM
  repair, or dedicated text steps. Never rewrite a file solely to
  change line endings.
