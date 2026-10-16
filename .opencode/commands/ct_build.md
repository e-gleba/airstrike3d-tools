---
description: Configure + build + test loop (llvm-mingw default, one retry on failure)
agent: build
---

Run the build loop for `$ARGUMENTS`. If empty, use the `llvm-mingw-i686` preset with the `-debug-with-tests` workflow.
Do not invent presets or `cmake -D` lines.

Follow `AGENTS.md` ## Session mechanics.

Available configure presets:

```text
!`cmake --list-presets 2>/dev/null | head -30`
```

Steps:
1. `cmake --preset <preset>`, then `cmake --build build/<preset> --config Debug -j`, then `ctest --test-dir build/<preset> --output-on-failure`.
2. Or in one shot: `cmake --workflow --preset <preset>-debug-with-tests`.
3. On failure: read the first error, fix the single root cause, retry once.
   A second failure is a report, not another fix attempt.
4. Never fall back to Release to "just check". CI `*-release` presets skip tests — use `-with-tests` locally (needs Steam + Proton for emulator tests).

Report (compact, no log dumps):
- preset, config, build OK/FAILED, tests passed/failed/skipped counts
- on failure: failing target + first error line + what the retry changed
