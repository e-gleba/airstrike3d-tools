# Compact project context

Keep continuity across tasks without loading history.

## Document roles

- `AGENTS.md` is behavior: build, CMake, C++, presets, deps, tests, quality, CI, cross, packaging.
- `.opencode/REVIEW.md` is mechanical shape: what the review agent checks.
- `readme.md` owns user-facing docs (engine history, formats, build, tests). No link duplication in agent files.
- Current request + current source win. Older context never overrides them.

## Selective reading

Start from the request, then `AGENTS.md`, then only the `cmake/presets/*.json`
or `cmake/toolchains/*.cmake` the request touches. Follow a dependency only
when the request makes it relevant. A link is not an instruction to load it.

For preset work, read the sibling preset file (e.g. `clang_windows.json`
for a native Windows variant, `llvm_mingw_linux.json` for a cross variant)
before inventing anything.
