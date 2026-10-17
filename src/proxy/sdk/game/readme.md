# Game Integration (`game/`)

Signature-located entry points into the running game image. Everything here
is pattern-based — no hard-coded virtual addresses — so one module covers all
engine variants shipped under `src/`.

## Files

| File                   | Purpose                            |
| ---------------------- | ---------------------------------- |
| `cheats.hpp`           | Public cheat-dispatch interface    |
| `detail/cheats_win32.cpp` | Runtime locator + dispatcher call |

## Cheat dispatcher scan (`detail/cheats_win32.cpp`)

Locates the native per-character cheat dispatcher (see `docs/re/cheats.md`
for the radare2/r2ghidra analysis):

1. resolve ≥5 of 7 cheat literals as full C strings in the largest
   initialized-data section (`.rdata` in the shipped builds; discardable
   sections are skipped);
2. find `push imm32` xrefs of each literal inside the executable section;
3. require the xref spread to fit one function body;
4. walk back from the earliest xref over a `C3 CC` boundary to a
   `83 EC 0C|10` prologue (v2.51/2.71/Space Strike stack sizes);
5. require every xref to walk back to the same entry.

The scan runs once, guarded by a mutex; on failure
(`cheats_available() == false`) callers fall back to `sdk.send_chars`
keystroke synthesis in Lua.

## Verification limits

The dispatcher call path cannot be exercised by CTest: the scan targets the
real game image, which the test harness does not reconstruct (v2.06's
ASProtect-packed exe has no cheat literals at all). Coverage is therefore:

- unit/build level — the SDK compiles, links and the lazy scan stays out of
  boot (nothing runs until the first `sdk.cheats_available()`);
- emulator launches (Proton) — the game boots with the proxy DLL loaded;
  note the legacy launches occasionally assert engine-internal exit 3 on
  this hardware/Proton combo (already documented by `run_with_proton.sh.in`
  with its `--wined3d` / `--no-laa` flags) — an immediate retry passes;
- end-to-end cheat triggering — manual play only (engine status text +
  `sounds\cheat.wav` at the moment the last character lands), as documented
  in `docs/re/cheats.md`.

