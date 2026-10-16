# Code Review Style Guide

Mechanical checks for `@ct_cmake_review` and `/ct_review`.
`AGENTS.md` owns all project rules; this file owns review procedure and
points at it. Where they conflict, `AGENTS.md` wins and this file gets fixed.

## Procedure

- Changed lines only. No drive-by edits. Where rules conflict with nearby
  code, follow existing code and report the conflict.
- Review in order: lifetime/ownership, UB, init/narrowing, concurrency,
  error paths, interface impact, measured perf, style last.
- Ground C++ claims in cppreference (mark the rest `[unverified]`).
  Perf claims need measurements. Ground CMake claims in
  `cmake/presets/*.json` or `cmake/toolchains/*.cmake`.
- Output `file_path:line_number` + rule + one-line fix, severity-sorted
  (error, warning, nit). End with a 3-line verdict:
  approve / approve-with-nits / request-changes.

## Checklist (rules live in AGENTS.md, not repeated here)

- CMake + presets + deps → `AGENTS.md` ## CMake, ## Presets, ## Deps.
- C++23 → `AGENTS.md` ## C++.
- Tests → `AGENTS.md` ## Tests.
- Workflows → `AGENTS.md` ## CI.
- Docs → `AGENTS.md` ## README, ## Text files.

## Shape (defined here, nowhere else)

- `[[nodiscard]]` on declarations, never repeated on out-of-class definitions.
- Split calls put one argument per line. Max one empty line. Operators open
  continuation lines.
- Includes: own header first, then alphabetical. No file-scope `using namespace`.
