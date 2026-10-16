---
description: Mechanical review of current diff via read-only reviewer (changed lines only)
agent: build
---

Review `$ARGUMENTS`. If empty, review the whole working tree vs HEAD.

Follow `AGENTS.md` ## Session mechanics.

```text
!`git status --short && git diff HEAD --stat`
```

Steps:
1. If the tree is clean, say so and stop — no review of committed history.
2. Delegate exactly one review pass to `@ct_cmake_review` with the diff
   (prompt is self-contained: files + `.opencode/REVIEW.md` procedure). Stay on the
   `build` agent yourself; never switch the session to the reviewer.
3. Present its verdict verbatim, then your one-line take. Never edit code
   as part of a review.

Apply `.opencode/REVIEW.md` exactly, in order: lifetime/ownership, UB,
init/narrowing, concurrency, error paths, interface impact, perf, style last.
- Changed lines only. No drive-by edits.
- Conflicts with nearby code resolve toward existing code, reported.
- Ground C++ claims in cppreference; mark unverified `[unverified]`.
  Perf claims need measurements.
- Output: `file_path:line_number` + rule + one-line fix, severity-sorted,
  with error/warning/nit counts. No pleasantries.
