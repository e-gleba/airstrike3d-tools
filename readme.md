# AirStrike 3D Reverse Engineering

**Reverse engineering the AirStrike 3D game series**

![gamelogo](.github/logo.jpg)

[PCGamingWiki](https://www.pcgamingwiki.com/wiki/AirStrike_2) - [Original Game](https://en.wikipedia.org/wiki/AirStrike_3D)

## 🎮 About

My nostalgic journey into reverse engineering AirStrike 3D - the first PC game that captured my imagination as a kid. This repository contains tools and research for understanding the game's internals.

![overlay preview](.github/overlay.png)

![overlay wireframe](.github/overlay_wireframe.png)

## 🕵️ About the Game

[AirStrike 3D](https://en.wikipedia.org/wiki/AirStrike_3D) is a helicopter shoot-em-up series developed by **[DivoGames](https://web.archive.org/web/2006/http://divogames.com/)** (Nizhny Novgorod, Russia) and published through **[Alawar Entertainment](https://en.wikipedia.org/wiki/Alawar)**. The engine and all three franchise titles were built by a two-person team.

### developers

| Name | Role | Links |
|------|------|-------|
| **Anton Petrov** | Engine architect, CTO & co-founder | [LinkedIn](https://www.linkedin.com/in/anton-petrov-cto/) |
| **Dmitry Zakharov** | Co-founder | — |

Both names are embedded as string literals (`{Anton Petrov}`, `{Dmitry Zakharov}`) in the Gulf Thunder executable's credits data. Petrov describes the engine on LinkedIn as _"my first game engine featuring a custom scripting language and hardware-accelerated 3D graphics — powered three titles in the Air Strike 3D franchise"_.

After DivoGames, Petrov became CTO at **Game Insight** (2012–2019, Nizhny Novgorod department), then co-founded **Colossi Games** in Cyprus (2020–present).

### deaddybear → divogames

Before DivoGames was officially founded (~2004), the initial AirStrike chapters were developed under a group called **Deaddybear**. Community [research on r/airstrike3d](https://www.reddit.com/r/airstrike3d/comments/16k254c/about_divogames_earlier_development_projects/) found that Deaddybear's earlier game _Treasure Mole_ used a nearly identical `.pak` archive format — confirming shared codebase ancestry. Deaddybear also released _Bomberman vs Digger_ (2002).

### franchise timeline

| Year | Title | Publisher | Genre | Engine | Known Alias |
|------|-------|-----------|-------|--------|-------------|
| 2002 | AirStrike 3D: Operation W.A.T. | Alawar | Helicopter shooter | v1.x (OpenGL, Deaddybear era) | *Air Assault 3D*, *Air Hawk* |
| 2004 | AirStrike 2 | Alawar / self | Helicopter shooter | v2.06 (OpenGL 1.1, MSVC 7.0) | *АвиаНалет 2* (ru) |
| 2005 | AirStrike II: Gulf Thunder | Alawar | Helicopter shooter | v2.71 (Direct3D 8, MSVC 8.0) | *Desert Hawk* |
| 2007 | Air Force Missions | MyPlayCity | Helicopter shooter | v2.50 (unconfirmed, same engine lineage) | — |
| 2007 | Space Strike | MyPlayCity | Space shooter | unknown | *Galaxy Strike*, *Звёздный Удар* |

> **Known retail rebrands** (same binary, different publisher skin):
> - *AirStrike 3D: Operation W.A.T.* → **"Air Assault 3D"** / **"Air Hawk"**
> - *AirStrike II: Gulf Thunder* → **"Desert Hawk"**
> - *Space Strike* → **"Galaxy Strike"**
>
> **Air Force Missions** and **Space Strike** are distinct DivoGames titles — separate from the Alawar-published trilogy — released in 2007 under a MyPlayCity distribution deal. Air Force Missions is a helicopter shooter sharing visible engine DNA with Operation W.A.T. (version string `2.50` observed in binary); Space Strike is a space shooter, unrelated gameplay-wise. Neither title's asset format compatibility with v2.06/v2.71 tooling has been confirmed — requires binary diff. Issue tracked at [#1](https://github.com/e-gleba/airstrike3d-tools/issues/1).
>
> DivoGames was acquired by **Game Insight** in 2012; both 2007 titles are now part of that catalog.

## 🔬 Engine Internals

Custom C++ engine with no third-party framework. Uses Quake-style subsystem prefixes:

| Subsystem | Prefix | Examples |
|-----------|--------|----------|
| Game logic | `G_` | `G_LoadBin`, `G_LoadLevelList` |
| Renderer | `R_` | `R_LoadModel`, `R_RegisterModel`, `R_RegisterShadow` |
| Sound | `S_` | `S_Init`, `S_RegisterSound` |
| Window | `MW_` | `MW_CreateWindow` |

### graphics api evolution

| Version | API | Compiler | Compile timestamp | Rich header |
|---------|-----|----------|-------------------|-------------|
| v2.06 (`as3d2.exe`) | OpenGL 1.1 (`opengl32.dll`, `glu32.dll`) | MSVC 7.0 (.NET 2002/2003) | `2004-05-15 10:12:58 UTC` | ✅ |
| v2.71 (`Gulf.exe`) | Direct3D 8 (`d3d8.dll`) | MSVC 8.0 (VS2005) | `2007-05-15 13:49:28 UTC` | ✅ |

### third-party libraries

- **[BASS](https://www.un4seen.com/)** — Audio library. 3D positional audio, EAX effects, MO3/tracker module playback.
- **libjpeg** — `Copyright (C) 1996, Thomas G. Lane` (found in Gulf exe strings).
- **zlib + libpng** — PNG texture support.
- **Custom scripting language** — Confirmed by Petrov on LinkedIn, no public documentation survived.

### asset formats

| Format | Extension | Description |
|--------|-----------|-------------|
| Archives | `.apk` | Custom encrypted containers (XOR, 1024-byte key table). **Not** Android APK. |
| Models | `.mdl` | Custom 3D format with version checks (`R_LoadModel: Illegal model version.`) |
| Textures | `.tga` | Standard Targa. Organized in `gfx/`, `menu/`, `tiles/` dirs. |
| Levels | `maps/levels.txt` | Plaintext level list (encrypted inside `.apk`) |
| Audio | `.mo3` | Tracker modules via BASS library |
| Config | `config.ini` | Plaintext, stored alongside the executable |

### rtti / c++ details

MSVC RTTI type descriptors found in the Gulf binary (e.g. `.?AVIntroPageDivoGames@@`), confirming C++ with virtual inheritance and RTTI enabled. `Divo Master` string suggests an internal tool or debug mode.

## 🔒 ASProtect 1.0 Analysis

The v2.06 executable (`as3d2.exe`, 199,680 bytes) is packed with **[ASProtect 1.0](http://asprotect.net)** by Alexey Solodovnikov.

### identification

| Indicator | Value | Meaning |
|-----------|-------|---------|
| Entry point | `.data` section (`0x1DB3001`) | Packer stub, not original code |
| EP signature | `60 E8 01 00 00 00` | `PUSHAD` + `CALL +1` — textbook ASProtect 1.0 |
| Section flags | All `0xC0000040` (RWX) | Packer rewrites all section attributes |
| `.text` entropy | **8.00** (maximum) | Fully encrypted/compressed |
| Visible IAT | 3 imports: `GetProcAddress`, `GetModuleHandleA`, `LoadLibraryA` | Real IAT resolved at runtime |
| Compression | aPLib (LZ77 variant) | See [`scripts/static_exe_unpacker.py`](scripts/static_exe_unpacker.py) |
| Hashes | `MD5: 1ba6f0187c43d07587e5212f1cb14190` | `SHA256: bc68bf37...81fb1a` |

### how it works

1. **Section wiping** — Original section names erased, all flags set to `0xC0000040`. Two `.data` stubs appended.
2. **aPLib decompression** — Compressed `.text` stored in oversized `.data` (VirtSize 30 MB, RawSize 4 KB).
3. **OEP byte stealing** — First bytes of Original Entry Point executed inside the stub before jumping to `OEP+N`.
4. **IAT redirection** — Import calls routed through ASProtect memory; executes first instructions of real API in-place, then jumps mid-body.
5. **Anti-debug** — `IsDebuggerPresent()`, RDTSC timing, SEH breakpoint detection, debugger driver `CreateFile()` probes.
6. **Checksums** — Code integrity verification to detect runtime patching.
7. **Anti-disasm** — Junk bytes after `CALL` instructions break linear-sweep disassemblers (W32DASM, SOURCER); IDA handles fine.

### v2.71 — no protection

Gulf Thunder ships **completely unprotected**: EP in `.text`, entropy 6.83, full IAT, developer credits and error strings plainly readable. Much better target for engine analysis.

## 🔗 Related Resources

- [r/airstrike3d](https://www.reddit.com/r/airstrike3d/) — community research & modding
- [Ithamar's APK scripts](https://gist.github.com/Ithamar/85f1f71d179c354fad483a8c48767daf) — updated extraction with text decryption
- [QindieGL](https://github.com/nicedrak/QindieGL) — OpenGL-to-D3D wrapper for running on modern Windows
- [PCGamingWiki: AirStrike 2](https://www.pcgamingwiki.com/wiki/AirStrike_2) — compatibility fixes
- [xakep.ru: ASProtect taming](https://xakep.ru/2003/07/10/19112/) — technical packer analysis (Russian)
- [ASProtect homepage](http://asprotect.net) — Alexey Solodovnikov's official site

## 🔧 Tools

### APK Archive Extraction

```bash
# Extract game assets from encrypted .apk archives
python extract_apk.py pak0.apk        # Extracts all files
python pack_apk.py extracted_dir/ new.apk  # Repack modified assets
```

### mdl to obj and vice versa

```bash
python mdl_obj_converter.py some_file.mdl
python mdl_obj_converter.py some_file.obj
```

### Save previewer (+imhex struct preview)

```bash
python decrypt_save.py decrypt game.bin -o decrypted.bin
```

### Audio Conversion

```bash
# Convert MO3 tracker modules to standard audio
sudo dnf install libopenmpt openmpt123
openmpt123 --render file.mo3 --output file.wav
```

### Graphics Viewing

```bash
# Best TGA texture viewer for Linux
# https://github.com/bluescan/tacentview
tacentview texture.tga
```

## 🐧 Linux Compatibility

### Running via Steam Proton (Fedora + AMD GPU)

```bash
# Fix OpenGL extension issues for old games
MESA_EXTENSION_MAX_YEAR=2003 %command%
```

Add this to the game's launch options in Steam.

## 📋 Technical Notes

- **Archive Format:** Custom encrypted APK containers (not Android APK)
- **Executable:** ASProtect v1.0 packed (detected via YARA rules)
- **Assets:** TGA textures, MDL 3D models, MO3 audio modules
- **Encryption:** XOR cipher with 1024-byte key table

## 🚀 Quick Start

1. Clone this repository
2. Extract game assets: `python extract_pak.py /path/to/pak0.apk`
3. Browse extracted files in the created directory
4. Convert audio files as needed

### build

1. download llvm-mingw from <https://github.com/mstorsjo/llvm-mingw/releases>:

- `llvm-mingw-YYYYMMDD-ucrt-ubuntu-20.04-x86_64.tar.xz` for win10+ (ucrt)
- `llvm-mingw-YYYYMMDD-msvcrt-ubuntu-20.04-x86_64.tar.xz` for win7+ (legacy crt)

1. extract to repository root in dir `llvm-mingw`

2. Run

```bash
cmake --preset llvm-mingw-i686
cmake --build --preset llvm-mingw-i686
```

> **note**: preset uses `jobs=1` due to lld linker deadlock on parallel linking in mingw context

## 🏴‍☠️ Ghidra Project

🔒 Since the project uses **ASProtect 1.0**, I decided on Linux using a simple debugger to just walk until we get some kind of loop. The game seems to unpack itself creating some thread, so even the debugger detaches at some moment in `ntdll` magic 🪄, so we need just to pause at any moment and get the address of the desired function (loop).

🎯 The next step is using **x64dbg** with **DumpEx** plugin—dump with the address of main loop function. And that's all!

📊 **Stats:**

- 📦 Game weights: **31.2 MB**
- 🔍 In Ghidra project I've marked some of the interesting places:
  - 🎮 Loading models
  - 💾 Working with saves
  - 🔧 Core game mechanics

🚀 **Usage:**

> Just clone and open with Ghidra—the project is ready to explore yourself!

Maybe some time someone will reverse it completely 😏 🦀⚡

## ⚖️ Legal

Educational and preservation purposes only. Respect original copyrights.

## 📄 License

MIT - Because knowledge should be free, just like the joy of playing games.

## 🙏 Acknowledgments

To that old PC that could barely run the game but somehow made it magical anyway.
