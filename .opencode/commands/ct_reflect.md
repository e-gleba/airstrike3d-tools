---
description: Learn from corrections — diff staged vs unstaged and distill one rule into AGENTS.md or REVIEW.md
agent: build
---

Reflect on corrections in `$ARGUMENTS`. If empty, reflect on the whole working tree (staged = what the agent wrote, unstaged = what the user fixed).

Follow `AGENTS.md` ## Session mechanics. Constraints from `AGENTS.md` (non-negotiable):

- Changed lines only. Never dump corrections verbatim — distill one principle.
- `AGENTS.md` owns behavior, `.opencode/REVIEW.md` owns shape; on conflict AGENTS.md wins.
- Propose via the native question tool first; edit only after approval.

```text
!`git status --short && git diff --cached --stat && git diff --stat`
```

Steps:
1. Run `git diff --cached` (agent's work) and `git diff` (user's corrections) in parallel. If either is empty, stop — reflection needs both.
2. Read `AGENTS.md` + `.opencode/REVIEW.md`. Per correction decide: already covered (skip), rule too narrow (broaden it), genuinely new (add it), or task-specific noise (no doc change, stop).
3. On contradiction with an existing rule, ask via the question tool how to reconcile — never silently append.
4. Quote the exact addition (file + location + why it generalizes), get approval via the question tool, then apply with one edit.
5. One insight per run maximum. Match the target file's tone; no meta-commentary about this run.

Failure ladder: empty diff, purely task-specific corrections, or rejected proposal = stop with a one-line reason, no edit.
Report: corrections (one line each), verdict (covered/broadened/added/noise), rule quote if proposed or applied.
