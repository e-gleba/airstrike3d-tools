---
description: Add a CPM dependency with pinned tag, license check, and link scope
agent: build
---

Add dependency `$ARGUMENTS` (required: library name + version).

Follow `AGENTS.md` ## Session mechanics. Rules from `AGENTS.md` (non-negotiable):

- CPM primary, no vcpkg overlay in this repo. Tagged release, never a branch. `FETCHCONTENT_QUIET OFF`.
- Options go inside `CPMAddPackage`, nowhere else.
- `find_package(CONFIG REQUIRED)` via a `cmake/cpm/` config; keep it
  reachable for cross via `CMAKE_FIND_ROOT_PATH`.
- Check `.github/renovate.json5` manager patterns first — a new pin form may need a regex there too.

Steps:
1. If `$ARGUMENTS` lacks the version, ask through the native question
   tool — propose the latest stable upstream tag as the recommended
   option — then continue. Never default to a branch or `main`.
2. Check the license first: MIT-compatible only (this repo is MIT).
   Incompatible license = stop and report, do not vendor.
3. Check `cmake/cpm-config.cmake` + `cmake/cpm/*-config.cmake` — reuse the
   pattern; refuse a second mechanism for one dep.
4. Add `CPMAddPackage` pinned to the tag, wire `target_link_libraries`
   (`PRIVATE` default; `PUBLIC` only with a stated propagation reason).
5. Verify: `cmake --preset llvm-mingw-i686` configures, then build the dependent
   target. Add the Origin URL + version + license comment at the call site.

Report: package, tag, license, target, link scope, configure/build OK/FAILED.
