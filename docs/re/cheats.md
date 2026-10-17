# Cheat dispatch — reverse engineering notes

Per-character cheat system shared by every AirStrike engine build shipped in
`src/` (2.51, 2.71 Gulf Thunder, Space Strike). Findings below were produced
with radare2 + the r2ghidra decompiler (`pdg`) on Fedora, cross-checking the
three PE images against each other.

AirStrike 3D v2.06 (`src/2_06/as3d2.exe`) has **no cheat system**: the ASProtect
packed stub encrypts its strings and none of the seven cheat literals appear
after de-obfuscation. It also pre-dates `sounds\cheat.wav`, which only exists
from v2.51 onward. The v2.06 build therefore stays on the SDK `send_chars`
fallback rather than the native dispatcher.

## The cheat dispatcher

The engine exposes one `__cdecl` function per character typed in-game, fed from
the WndProc `WM_CHAR` path. Every build embeds the same layout:

1. **first call** — zero-flip of a "buffer initialized" flag, then fills a
   32-byte keystroke buffer with `0x20202020` (`'    '`).
2. **append + trim** — `strncat(dst=buffer+1, src=buffer, 31)` recompacts, then
   lowercases the newest char into the last byte of the buffer.
3. **match** — `fstricmp(buffer, "cheat literal")` for each of the seven
   cheats; first hit applies the effect in-line (lives arrays, ammo slots,
   god-mode flag, power-up counters, win/lose fns, sound event).
4. **acknowledge** — writes the "God Mode: Enabled"-style message into a
   status-text pointer, plays `sounds\cheat.wav`, clears the buffer back to
   `0x20202020` and raises a one-frame "cheat activated" flag consumed by the
   HUD.

Calling the dispatcher once per character reproduces typing a cheat without
synthetic keyboard input; when the last character lands, the effects fire and
the game itself resets the buffer.

## Engine builds

### v2.51 — `src/2_51/as3d2.exe` (OpenGL)

| Item | VA |
|---|---|
| dispatcher（`__cdecl(int)`） | `0x00407110` |
| prologue | `83 EC 0C 80 3D .. .. 21 02 00` (`sub esp,0xC; cmp byte [0x221913B],0`) |
| dispatcher end | `0x004077A4` (`ret`, `CC`-padded after) |
| first cheat push site | `0x00407179` (`push str.invulnerability`) |
| cheat literal table | `.rdata` `0x0048A008`…`0x0048A0F8` |
| keystroke buffer | `0x004C2000` (32 B, `'    '`-filled) |
| buffer init flag | `0x00221913B` |
| god-mode flag | `0x00221913A` |
| status text ptr | `0x0049F9FC` (`"God Mode: Enabled"` …) |
| cheat-ammo arrays | `0x020C5BEC` (`weapons[10]`), `0x020C5D50` (`missiles[10]`) |
| cheat lives destination | `0x020C5B6C`, `0x020C5CD0` (floats) |
| cheat lives source const | `0x0048F880` (`10.0`) |
| power-up counters | `0x020C5B90`, `0x020C5B94`, `0x020C5BD0`, `0x020C5D4C` |
| win / lose handler | `0x0040E730` / `0x00410E70` |
| caller (WndProc gate) | `0x004110DC`: `push ebx; call 0x407110; add esp,4` — active when a menu byte `0x0054329A` is zero |

`cheats` block of the v2.51 read:

```
0x0048A008 "invulnerability"      0x0048A040 "igonnaliveforever"
0x0048A068 "showmetheweapons"     0x0048A094 "moremoreweapons"
0x0048A0BC "glitteringprizes"     0x0048A0E8 "deadlineisnear"
0x0048A0F8 "diediediemydarling"
```

### v2.71 — `src/2_71/as3d_gulf.exe` (Direct3D 8)

| Item | VA |
|---|---|
| dispatcher（`__cdecl(int)`） | `0x00407070` |
| prologue | `83 EC 0C 80 3D 83 6F 21 02 00` (`sub esp,0xC; cmp byte [0x2216F83],0`) |
| dispatcher end | `0x0040769C` (`ret`, `CC`-padded after) |
| first cheat push site | `0x004070D9` (`push str.invulnerability`) |
| cheat literal table | `.rdata` `0x00488FE8`…`0x004890D8` |
| keystroke buffer | `0x004BFE40` (32 B, `'    '`-filled) |
| buffer init flag | `0x002216F83` |
| god-mode flag | `0x002216F82` |
| status text ptr | `0x0049D83C` |
| cheat lives destination | `0x0020C5BEC`-family (shifted −0x4 vs 2.51) |
| status fn (engine msg) | `0x0041C7D0` |

`alt string table` (for the `fstricmp` literal checks):

```
0x00488FE8 "invulnerability"      0x00489020 "igonnaliveforever"
0x00489048 "showmetheweapons"     0x00489074 "moremoreweapons"
0x0048909C "glitteringprizes"     0x004890C8 "deadlineisnear"
0x004890D8 "diediediemydarling"
```

### Space Strike — `src/space_strike/game.exe` (Direct3D 8)

| Item | VA |
|---|---|
| dispatcher（`__cdecl(int)`） | `0x0041C9D0` |
| prologue | `83 EC 10 80 3D 92 96 2D 02 00` (`sub esp,0x10; cmp byte [0x22D9692],0`) |
| dispatcher end | `0x0041CF03` (`ret`, `CC`-padded after) |
| first cheat push site | `0x0041CA3A` (`push str.invulnerability`) |
| cheat literal table | `.rdata` `0x00499B08`…`0x00499BF8` |
| keystroke buffer | `0x004AFF0C` (32 B, `'    '`-filled) |
| buffer init flag | `0x0022D9692` |
| god-mode flag | `0x0022D9691` |
| status text ptr | `0x0049D83C` |
| cheat-ammo arrays | `0x022AB660` base (engine sound event table) |
| win / lose handler | `0x00408E70` (`diediediemydarling` reverse-side) |
| status fn (engine msg) | `0x041C7D0`-analogue `0x0041C7D0` |
| caller (WndProc gate) | `0x0042565C`: `push edi; call 0x41C9D0; add esp,4` — active when a menu byte `0x004B219D` is zero |

## r2 / r2ghidra walkthrough (reproducible)

Toolchain used on Fedora:

```bash
export LD_LIBRARY_PATH="$HOME/.local/lib"   # user-local radare2 6.1.4 + r2ghidra
r2 -q -c 'e bin.cache=true; e scr.color=0; izz~.invulnerability' src/2_51/as3d2.exe
r2 -q -c 'e bin.cache=true; e scr.color=0; /r 0x0048A008'        src/2_51/as3d2.exe
r2 -q -c 'e scr.color=0; s 0x407110; af; pdg'                    src/2_51/as3d2.exe
```

Key steps:

1. **find the literals** — `izz~cheat`/`izz~.invulnerability` locates `.rdata`
   cheat strings (v2.71 + Space Strike are unprotected; v2.06 is packed).
2. **find the reader** — `/r <string VA>` prints the push site(s) that
   reference the literal inside `.text`.
3. **walk back to the function start** — from the *first* push site scan
   backward for a `C3` boundary followed by an int3 (`CC`) padding run of
   any length, then require `83 EC imm8`; that byte is the dispatcher
   prologue. Verified start bytes:
   - v2.51 `0x407110` `83 EC 0C`
   - v2.71 `0x407070` `83 EC 0C`
   - Space Strike `0x41C9D0` `83 EC 10`
4. **confirm calling convention** — calling site shape is
   `push <char>; call <dispatcher>; add esp, 4`, so the per-char arg is a
   stack arg and the callee ends with a plain `ret` → `__cdecl(int32_t)`.
   This is what `sdk::game::cheat::dispatch_cheat_char()` implements.
5. **decompile the effect tree** — `pdg` at the dispatcher start exposes the
   extended effect body (god-mode sete, lives array copies, ammo fild/fstp
   arrays, win/lose handlers) and the acknowledge tail
   (buffer zero-fill, `cheat.wav`, sound-event, status text).

## Why the SDK calls it instead of typing

`lua/sdk.send_chars()` synthesizes Windows keystrokes (needs focus, risks
menu interaction, misses the audio ack). The SDK instead re-enters the same
engine function the keyboard path uses — the effect tree, `cheat.wav`, HUD
status text and the buffer reset all stay engine-native, and no fake WM_CHAR
hits ride along.

The scan is *pattern-based* (no hard-coded VAs):

- resolve the seven cheat literals in `.rdata` by byte sequence;
- find `push imm32` xrefs in `.text`;
- walk back from the first xref to a `C3 CC...` boundary and require the
  `83 EC 0C|10` prologue;
- require at least five of seven strings to resolve to the *same* function —
  if fewer match, treat the image as "not supported" (v2.06 / future builds).

Once located, `sdk::game::cheat::dispatch_cheat_char(int32)` forwards one key
at a time; `sdk::game::cheat::cheats_available()` reports whether the
specialized scan found a dispatcher in the loaded module. Each Lua call is
one-shot from the render thread (the game only dispatches cheats there).
