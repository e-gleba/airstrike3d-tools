---
description: Intent-aware rebase onto a ref, resolving conflicts by reading both sides' history
agent: build
---

Rebase `$ARGUMENTS` (required: `onto <ref>`, `<branch> onto <ref>`, `tail of <branch> on top of <ref>`, `continue`, or `abort`). If empty, ask instead of guessing.

Follow `AGENTS.md` ## Session mechanics. Constraints from `AGENTS.md` (non-negotiable):

- Never invent refs; ground every one with `git rev-parse --verify`.
- No delegation — resolution needs both sides' history in one context.
- Non-interactive git only: `git -c core.editor=true` for `rebase --continue`, `GIT_SEQUENCE_EDITOR` for todo edits.

```text
!`git status --short && git branch --show-current && git log --oneline -8`
```

Steps:
1. If `$ARGUMENTS` lacks a ref or is ambiguous, ask through the native question tool with concrete options from `git branch -a` + `git tag --sort=-v:refname`, then continue. `continue`/`abort` pass straight through.
2. Build the replay list (`git log --oneline <base>..HEAD`); read each commit's intent from its message + `git show --stat` before touching conflicts.
3. Replay in order; per conflict read both sides (`git log -p` on each range), resolve toward the commit's stated intent, `git add` the paths, continue non-interactively.
4. On incompatible intents on the same lines, stop and ask via the question tool with 2-4 concrete directions — never continue past an unanswered question.
5. Verify: `git status` free of conflicts, then reconfigure the touched preset (`cmake --preset llvm-mingw-i686` on Linux trees).

Failure ladder: one resolution retry per commit; unresolvable intent = stop and offer `git rebase --abort`, never force it.
Report: refs, commits replayed, one resolution line per commit, verification OK/FAILED + first error line.
