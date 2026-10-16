# `.opencode` Conventions

Rules for adding commands, agents, and skills in this repo.
`AGENTS.md` owns project behavior; this file owns prompt-file shape.

## Naming

- `ct_` prefix + `snake_case`, always. The prefix marks project-owned
  files apart from vendored externals (`.agents/skills/*` keep upstream
  names) and from opencode builtins.
- Commands: verb-first (`ct_build`, `ct_add_dep`, `ct_cross_check`).
- Agents: role nouns (`ct_cmake_review`). Never reuse a command name.

## Command vs agent vs skill

- Command (`.opencode/commands/*.md`): a repeatable task the user invokes
  with `/`. Runs in the current session. Prefer a command when the user
  would otherwise paste the same prompt twice.
- Agent (`.opencode/agents/*.md`): a role with its own tools/permissions,
  invoked via `@` or the `subagent` tool. Prefer an agent when the work
  needs different permissions than the caller (e.g. read-only reviewer
  with `edit: deny`).
- Skill (`.agents/skills/*`): third-party knowledge vendored via
  `.agents/install_skills.cmake` + pinned `skills-lock.json`. Vendor one
  only if this repo can build a test that triggers it; otherwise use
  `find-skills` on demand.

## Every command file

Frontmatter: `description` (verb + scope, shows in autocomplete) and
`agent` (almost always `build`; a `subagent`-mode agent would hijack the
session — delegate to it instead, see `ct_review.md`).
Never pin `model`: the user's model choice wins.

Body, in this order:

1. One line: what it does + `$ARGUMENTS` with its default — or marked
   `required` with the expected shape (`compiler + platform + arch`) —
   or no arguments at all for interactive commands (auth-style: check,
   ask via question tool, act). No bare `$ARGUMENTS` without either.
   Empty substitution is the common case, so state the empty behavior
   explicitly (`If empty, …`).
2. Link `AGENTS.md` ## Session mechanics (one line, always).
3. Constraints before steps: the non-negotiable `AGENTS.md` rules, quoted
   short. A command restates at most 5 rules and never duplicates
   `.opencode/REVIEW.md` or `ct_test_loop.md` — it links them.
4. Numbered steps, each verifiable (a command to run or a file to read).
   Prefer `!` shell blocks that ground the run
   (`cmake --list-presets`, `git status --short`) over hardcoded names.
5. Failure ladder: what counts as failure, one retry max, then report.
6. Report contract: exact fields, compact, no log dumps.
7. Missing input: never guess, never stall with a bare text question. Ask
   through the native question tool with 2-4 concrete options grounded in
   the repo (sibling presets, known tags) plus free text, then continue
   the same run. The session stays live; the question is a pause, not an end.

## Every agent file

Frontmatter: `description` (when the caller should pick it), `mode`
(`subagent` default; `primary` only with a stated reason), least-privilege
`permission` (`edit: deny` for reviewers).
Body: role in one line, required reading list, procedure, output format
with verdict vocabulary. Under 25 lines.
