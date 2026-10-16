---
description: Cut a release — dispatch publish_release.yml with version + next version
agent: build
---

Release `$ARGUMENTS`. If empty, auto-increment the patch as stable; `pre` marks it pre-release, an explicit `vX.Y.Z` overrides the number.

Follow `AGENTS.md` ## Session mechanics. Constraints from `AGENTS.md` (non-negotiable):

- Version truth is `project(VERSION)` in root `CMakeLists.txt`; tags are `v<version>`.
- `git status --porcelain` must be clean before starting — otherwise stop.
- This repo has no CHANGELOG file: release notes are generated at publish time (`publish_release.yml` → `gh releases generate-notes` + curated header). Never invent a changelog file.

```text
!`git status --short && git tag --sort=-v:refname | head -5 && grep -n "project(" CMakeLists.txt | head -5`
```

Steps:
1. Resolve the version (explicit arg wins, else patch+1; a pre-release never reuses a stable number). Confirm it via the native question tool before doing anything.
2. Confirm `next_version` too (plain semver, no `v` prefix, must be greater than current `project(VERSION)`).
3. Verify both workflow presets configure: `cmake --preset llvm-mingw-i686` (Linux check always runs); `clang_windows_x86`/`msvc` only on Windows runners.
4. Do NOT tag locally. Give the user the one-click dispatch link with inputs filled:
   `publish_release.yml`: `version=vX.Y.Z`, `next_version=X.Y.Z+1`, `prerelease=true/false`.
   The workflow builds both packages, tags, publishes the GitHub Release, then opens+merges the `project(VERSION)` bump PR itself.

Failure ladder: dirty tree, ambiguous version, or failed configure = stop and report, no dispatch.
Report: version, next_version, dispatch link, configure OK/FAILED. No log dumps.
