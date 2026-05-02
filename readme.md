<p align="center">
  <img src=".github/logo.jpg" width="180" alt="AirStrike 3D Logo">
</p>

<h1 align="center">AirStrike 3D — Reverse Engineering Toolkit</h1>

<p align="center">
  <strong>Reverse engineering the AirStrike 3D game series</strong>
</p>

<p align="center">
  <a href="https://github.com/e-gleba/airstrike3d-tools/blob/main/license">
    <img src="https://img.shields.io/badge/License-MIT-yellow.svg" alt="License MIT">
  </a>
  <a href="#build--development">
    <img src="https://img.shields.io/badge/CMake-3.31+-064F8C?logo=cmake&logoColor=white" alt="CMake 3.31+">
  </a>
  <a href="#engine-internals">
    <img src="https://img.shields.io/badge/C++-26-00599C?logo=c%2B%2B&logoColor=white" alt="C++26">
  </a>
  <a href="#toolkit">
    <img src="https://img.shields.io/badge/Python-3.x-3776AB?logo=python&logoColor=white" alt="Python 3.x">
  </a>
  <a href="https://github.com/e-gleba/airstrike3d-tools/issues">
    <img src="https://img.shields.io/github/issues/e-gleba/airstrike3d-tools" alt="GitHub Issues">
  </a>
</p>

<p align="center">
  <a href="https://www.pcgamingwiki.com/wiki/AirStrike_2">PCGamingWiki</a> ·
  <a href="https://en.wikipedia.org/wiki/AirStrike_3D">Original Game</a> ·
  <a href="https://www.reddit.com/r/airstrike3d/">Community (Reddit)</a>
</p>

---

## Table of Contents

- [Overview](#overview)
- [Project Status](#project-status)
- [Repository Structure](#repository-structure)
- [About the Game](#about-the-game)
  - [Developers](#developers)
  - [Deaddybear → DivoGames](#deaddybear--divogames)
  - [Franchise Timeline](#franchise-timeline)
- [Engine Internals](#engine-internals)
  - [Subsystem Naming](#subsystem-naming)
  - [Graphics API Evolution](#graphics-api-evolution)
  - [Third-Party Libraries](#third-party-libraries)
  - [Asset Formats](#asset-formats)
  - [RTTI / C++ Details](#rtti--c-details)
- [ASProtect 1.0 Analysis](#asprotect-10-analysis)
  - [Identification](#identification)
  - [How It Works](#how-it-works)
  - [v2.71 — No Protection](#v271--no-protection)
- [Toolkit](#toolkit)
  - [APK Archive Extraction](#apk-archive-extraction)
  - [MDL ↔ OBJ Converter](#mdl--obj-converter)
  - [Save Previewer](#save-previewer)
  - [Audio Conversion](#audio-conversion)
  - [Graphics Viewing](#graphics-viewing)
  - [Linux Compatibility](#linux-compatibility)
  - [Technical Notes](#technical-notes)
- [Build & Development](#build--development)
  - [Prerequisites](#prerequisites)
  - [Quick Start](#quick-start)
  - [Building C++ Components](#building-c-components)
- [Ghidra Project](#ghidra-project)
- [Contributing](#contributing)
- [Legal Notice](#legal-notice)
- [License](#license)
- [Acknowledgments](#acknowledgments)

---

## Overview

My nostalgic journey into reverse engineering AirStrike 3D — the first PC game that captured my imagination as a kid. This repository contains tools and research for understanding the game's internals.

![overlay preview](.github/overlay.png)

![overlay wireframe](.github/overlay_wireframe.png)

---

## Project Status

> **Active research & tooling.**  
> This project is a living archive. Engine analysis is ongoing, new tools are added as formats are documented, and the Ghidra database is updated with fresh discoveries. Contributions from fellow reverse engineers and preservationists are welcome.

| Milestone | Status |
|-----------|--------|
| `.apk` archive format | ✅ Documented & tooling complete |
| `.mdl` model format | ✅ Bidirectional converter |
| Save file format | ✅ Decryption + preview |
| ASProtect 1.0 unpacking | ✅ Static unpacker + manual guide |
| Engine v2.06 analysis | 🔄 In progress (Ghidra) |
| Engine v2.71 analysis | 🔄 In progress (Ghidra) |
| v2.50 / Air Force Missions | ⏳ Pending (tracked in [#1](https://github.com/e-gleba/airstrike3d-tools/issues/1)) |

---

## Repository Structure

```
airstrike3d-tools/
├── .github/              # Branding assets & templates
├── 2_06/                 # AirStrike 2 (engine v2.06) binaries & data
├── 2_71/                 # Gulf Thunder (engine v2.71) binaries & data
├── cmake/                # CMake modules, toolchains & code-quality configs
├── external/             # Vendored dependencies (GLAD, etc.)
├── ghidra/               # Ghidra project files for both game versions
├── scripts/              # Python tooling
│   ├── level_viewer.py
│   ├── mdl_obj_converter.py
│   ├── paktool.py
│   ├── save_editor.py
│   └── static_exe_unpacker.py
└── src/                  # C++ source code
    ├── game/             # Decompiled/reconstructed game logic (C)
    └── proxy/            # BASS proxy DLL for runtime injection & overlay
```

---

## About the Game

[AirStrike 3D](https://en.wikipedia.org/wiki/AirStrike_3D) is a helicopter shoot-em-up series developed by **[DivoGames](https://web.archive.org/web/2006/http://divogames.com/)** (Nizhny Novgorod, Russia) and published through **[Alawar Entertainment](https://en.wikipedia.org/wiki/Alawar)**. The engine and all three franchise titles were built by a two-person team.

### Developers

| Name | Role | Links |
|------|------|-------|
| **Anton Petrov** | Engine architect, CTO & co-founder | [LinkedIn](https://www.linkedin.com/in/anton-petrov-cto/) |
| **Dmitry Zakharov** | Co-founder | — |

Both names are embedded as string literals (`{Anton Petrov}`, `{Dmitry Zakharov}`) in the Gulf Thunder executable's credits data. Petrov describes the engine on LinkedIn as _"my first game engine featuring a custom scripting language and hardware-accelerated 3D graphics — powered three titles in the Air Strike 3D franchise"_.

After DivoGames, Petrov became CTO at **Game Insight** (2012–2019, Nizhny Novgorod department), then co-founded **Colossi Games** in Cyprus (2020–present).

### Deaddybear → DivoGames

Before DivoGames was officially founded (~2004), the initial AirStrike chapters were developed under a group called **Deaddybear**. Community [research on r/airstrike3d](https://www.reddit.com/r/airstrike3d/comments/16k254c/about_divogames_earlier_development_projects/) found that Deaddybear's earlier game _Treasure Mole_ used a nearly identical `.pak` archive format — confirming shared codebase ancestry. Deaddybear also released _Bomberman vs Digger_ (2002).

### Franchise Timeline

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

```mermaid
graph TD
    %% People
    AP["👤 Anton Petrov\nEngine Architect · CTO · Co-founder"]
    DZ["👤 Dmitry Zakharov\nCo-founder"]

    %% Orgs / Groups
    DB["🐻 Deaddybear\n~2000–2004"]
    DG["🏢 DivoGames Ltd.\nNizhny Novgorod · 2004–2012"]
    GI["🏢 Game Insight NN\nNizhny Novgorod · 2012–2019"]
    CG["🏢 Colossi Games\nCyprus · 2020–present"]
    AL["📦 Alawar Entertainment\nPublisher"]
    MPC["📦 MyPlayCity\nPublisher"]

    %% Engine lineage
    ENG1["⚙️ Engine v1.x\nOpenGL · Deaddybear era"]
    ENG206["⚙️ Engine v2.06\nOpenGL 1.1 · MSVC 7.0\ncompiled 2004-05-15"]
    ENG250["⚙️ Engine v2.50\nunconfirmed · same lineage"]
    ENG271["⚙️ Engine v2.71\nDirect3D 8 · MSVC 8.0\ncompiled 2007-05-15"]

    %% Games
    TM["🎮 Treasure Mole\n.pak format — shared codebase"]
    BVD["🎮 Bomberman vs Digger\n2002"]
    AS1["🎮 AirStrike 3D: Op. W.A.T.\n2002"]
    AS2["🎮 AirStrike 2\n2004"]
    GT["🎮 AirStrike II: Gulf Thunder\n2005"]
    AFM["🎮 Air Force Missions\n2007"]
    SS["🎮 Space Strike\n2007"]

    %% Aliases
    AA["🏷️ Air Assault 3D\nAir Hawk"]
    DH["🏷️ Desert Hawk"]
    GS["🏷️ Galaxy Strike\nЗвёздный Удар"]

    %% Evidence nodes
    EV1["🔍 EVIDENCE\nString literals in Gulf.exe:\n{Anton Petrov} {Dmitry Zakharov}"]
    EV2["🔍 EVIDENCE\nLinkedIn: 'my first game engine\npowered three titles'"]
    EV3["🔍 EVIDENCE\nMSVC RTTI: .?AVIntroPageDivoGames@@\nDivo Master debug string"]
    EV4["🔍 EVIDENCE\nr/airstrike3d research:\n.pak format shared with Treasure Mole"]

    %% Acquisition
    ACQ["📋 Acquisition 2012\nGame Insight buys DivoGames"]

    %% People → Orgs
    AP --> DB
    DZ --> DB
    DB -->|"~2004 rebranded/founded"| DG
    AP -->|"CTO · co-founder"| DG
    DZ -->|"co-founder"| DG
    AP -->|"CTO 2012–2019"| GI
    AP -->|"co-founded 2020"| CG
    DG -->|"acquired by"| ACQ
    ACQ --> GI

    %% Evidence links
    EV1 -.->|"confirms"| AP
    EV1 -.->|"confirms"| DZ
    EV2 -.->|"confirms"| AP
    EV3 -.->|"confirms"| DG
    EV4 -.->|"confirms"| DB

    %% Engine lineage
    ENG1 -->|"evolved to"| ENG206
    ENG206 -->|"evolved to"| ENG271
    ENG206 -.->|"possible fork"| ENG250

    %% Games → Engine
    AS1 --> ENG1
    AS2 --> ENG206
    GT --> ENG271
    AFM --> ENG250
    SS -.->|"engine unknown"| DG

    %% Deaddybear games
    DB --> TM
    DB --> BVD
    DB --> AS1

    %% DivoGames games
    DG --> AS2
    DG --> GT
    DG --> AFM
    DG --> SS

    %% Publishers
    AL -->|"published"| AS1
    AL -->|"published"| AS2
    AL -->|"published"| GT
    MPC -->|"published"| AFM
    MPC -->|"published"| SS

    %% Aliases
    AS1 -.->|"rebrand"| AA
    GT -.->|"rebrand"| DH
    SS -.->|"rebrand"| GS

    %% Styling
    classDef person fill:#1a3a5c,stroke:#4a9eda,color:#e8f4fd
    classDef org fill:#1a2a1a,stroke:#4aaa4a,color:#e8fde8
    classDef engine fill:#2a1a3a,stroke:#9a4aed,color:#f0e8fd
    classDef game fill:#2a1a1a,stroke:#ed6a4a,color:#fde8e8
    classDef alias fill:#1a2a2a,stroke:#4aaaaa,color:#e8fdfd,stroke-dasharray:4 2
    classDef evidence fill:#2a2a1a,stroke:#aaa04a,color:#fdfde8,stroke-dasharray:2 2
    classDef event fill:#2a1a2a,stroke:#aa4a6a,color:#fde8f0

    class AP,DZ person
    class DB,DG,GI,CG,AL,MPC org
    class ENG1,ENG206,ENG250,ENG271 engine
    class TM,BVD,AS1,AS2,GT,AFM,SS game
    class AA,DH,GS alias
    class EV1,EV2,EV3,EV4 evidence
    class ACQ event
```

---

## Engine Internals

Custom C++ engine with no third-party framework. Uses Quake-style subsystem prefixes:

### Subsystem Naming

| Subsystem | Prefix | Examples |
|-----------|--------|----------|
| Game logic | `G_` | `G_LoadBin`, `G_LoadLevelList` |
| Renderer | `R_` | `R_LoadModel`, `R_RegisterModel`, `R_RegisterShadow` |
| Sound | `S_` | `S_Init`, `S_RegisterSound` |
| Window | `MW_` | `MW_CreateWindow` |

### Graphics API Evolution

| Version | API | Compiler | Compile timestamp | Rich header |
|---------|-----|----------|-------------------|-------------|
| v2.06 (`as3d2.exe`) | OpenGL 1.1 (`opengl32.dll`, `glu32.dll`) | MSVC 7.0 (.NET 2002/2003) | `2004-05-15 10:12:58 UTC` | ✅ |
| v2.71 (`Gulf.exe`) | Direct3D 8 (`d3d8.dll`) | MSVC 8.0 (VS2005) | `2007-05-15 13:49:28 UTC` | ✅ |

### Third-Party Libraries

- **[BASS](https://www.un4seen.com/)** — Audio library. 3D positional audio, EAX effects, MO3/tracker module playback.
- **libjpeg** — `Copyright (C) 1996, Thomas G. Lane` (found in Gulf exe strings).
- **zlib + libpng** — PNG texture support.
- **Custom scripting language** — Confirmed by Petrov on LinkedIn, no public documentation survived.

### Asset Formats

| Format | Extension | Description |
|--------|-----------|-------------|
| Archives | `.apk` | Custom encrypted containers (XOR, 1024-byte key table). **Not** Android APK. |
| Models | `.mdl` | Custom 3D format with version checks (`R_LoadModel: Illegal model version.`) |
| Textures | `.tga` | Standard Targa. Organized in `gfx/`, `menu/`, `tiles/` dirs. |
| Levels | `maps/levels.txt` | Plaintext level list (encrypted inside `.apk`) |
| Audio | `.mo3` | Tracker modules via BASS library |
| Config | `config.ini` | Plaintext, stored alongside the executable |

### RTTI / C++ Details

MSVC RTTI type descriptors found in the Gulf binary (e.g. `.?AVIntroPageDivoGames@@`), confirming C++ with virtual inheritance and RTTI enabled. `Divo Master` string suggests an internal tool or debug mode.

---

## ASProtect 1.0 Analysis

The v2.06 executable (`as3d2.exe`, 199,680 bytes) is packed with **[ASProtect 1.0](http://asprotect.net)** by Alexey Solodovnikov.

### Identification

| Indicator | Value | Meaning |
|-----------|-------|---------|
| Entry point | `.data` section (`0x1DB3001`) | Packer stub, not original code |
| EP signature | `60 E8 01 00 00 00` | `PUSHAD` + `CALL +1` — textbook ASProtect 1.0 |
| Section flags | All `0xC0000040` (RWX) | Packer rewrites all section attributes |
| `.text` entropy | **8.00** (maximum) | Fully encrypted/compressed |
| Visible IAT | 3 imports: `GetProcAddress`, `GetModuleHandleA`, `LoadLibraryA` | Real IAT resolved at runtime |
| Compression | aPLib (LZ77 variant) | See [`scripts/static_exe_unpacker.py`](scripts/static_exe_unpacker.py) |
| Hashes | `MD5: 1ba6f0187c43d07587e5212f1cb14190` | `SHA256: bc68bf37...81fb1a` |

### How It Works

1. **Section wiping** — Original section names erased, all flags set to `0xC0000040`. Two `.data` stubs appended.
2. **aPLib decompression** — Compressed `.text` stored in oversized `.data` (VirtSize 30 MB, RawSize 4 KB).
3. **OEP byte stealing** — First bytes of Original Entry Point executed inside the stub before jumping to `OEP+N`.
4. **IAT redirection** — Import calls routed through ASProtect memory; executes first instructions of real API in-place, then jumps mid-body.
5. **Anti-debug** — `IsDebuggerPresent()`, RDTSC timing, SEH breakpoint detection, debugger driver `CreateFile()` probes.
6. **Checksums** — Code integrity verification to detect runtime patching.
7. **Anti-disasm** — Junk bytes after `CALL` instructions break linear-sweep disassemblers (W32DASM, SOURCER); IDA handles fine.

### v2.71 — No Protection

Gulf Thunder ships **completely unprotected**: EP in `.text`, entropy 6.83, full IAT, developer credits and error strings plainly readable. Much better target for engine analysis.

---

## Toolkit

### APK Archive Extraction

```bash
# Extract game assets from encrypted .apk archives
python extract_apk.py pak0.apk        # Extracts all files
python pack_apk.py extracted_dir/ new.apk  # Repack modified assets
```

### MDL ↔ OBJ Converter

```bash
python mdl_obj_converter.py some_file.mdl
python mdl_obj_converter.py some_file.obj
```

### Save Previewer (+ ImHex Struct Preview)

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

### Linux Compatibility

#### Running via Steam Proton (Fedora + AMD GPU)

```bash
# Fix OpenGL extension issues for old games
MESA_EXTENSION_MAX_YEAR=2003 %command%
```

Add this to the game's launch options in Steam.

### Technical Notes

- **Archive Format:** Custom encrypted APK containers (not Android APK)
- **Executable:** ASProtect v1.0 packed (detected via YARA rules)
- **Assets:** TGA textures, MDL 3D models, MO3 audio modules
- **Encryption:** XOR cipher with 1024-byte key table

---

## Build & Development

### Prerequisites

- **CMake** 3.31 or newer
- **Python** 3.x
- **Ninja** (used by presets)
- **LLVM-MinGW** (for Linux → Windows cross-compilation) or **Visual Studio 2022** (for native Windows builds)

### Quick Start

1. Clone this repository
2. Extract game assets: `python extract_pak.py /path/to/pak0.apk`
3. Browse extracted files in the created directory
4. Convert audio files as needed

### Building C++ Components

1. Download **llvm-mingw** from [mstorsjo/llvm-mingw releases](https://github.com/mstorsjo/llvm-mingw/releases):
   - `llvm-mingw-YYYYMMDD-ucrt-ubuntu-20.04-x86_64.tar.xz` for Windows 10+ (UCRT)
   - `llvm-mingw-YYYYMMDD-msvcrt-ubuntu-20.04-x86_64.tar.xz` for Windows 7+ (legacy CRT)

2. Extract to repository root in directory `llvm-mingw`

3. Run:

```bash
cmake --preset llvm-mingw-i686
cmake --build --preset llvm-mingw-i686
```

> **Note:** The preset uses `jobs=1` due to an LLD linker deadlock on parallel linking in the MinGW context.

For native Windows builds with Visual Studio 2022:

```bash
cmake --preset msvc
cmake --build --preset msvc
```

---

## Ghidra Project

🔒 Since the project uses **ASProtect 1.0**, I decided on Linux using a simple debugger to just walk until we get some kind of loop. The game seems to unpack itself creating some thread, so even the debugger detaches at some moment in `ntdll` magic 🪄, so we need just to pause at any moment and get the address of the desired function (loop).

🎯 The next step is using **x64dbg** with **DumpEx** plugin — dump with the address of main loop function. And that's all!

📊 **Stats:**

- 📦 Game weights: **31.2 MB**
- 🔍 In the Ghidra project I have marked some of the interesting places:
  - 🎮 Loading models
  - 💾 Working with saves
  - 🔧 Core game mechanics

🚀 **Usage:**

> Just clone and open with Ghidra — the project is ready to explore yourself!

Maybe some time someone will reverse it completely 😏 🦀⚡

---

## Contributing

We welcome contributions from reverse engineers, preservationists, and enthusiasts. Please see [`CONTRIBUTING.md`](CONTRIBUTING.md) for guidelines on coding standards, commit conventions, and the pull request workflow.

---

## Legal Notice

**Educational and preservation purposes only. Respect original copyrights.**

This project is intended for research, education, and game preservation. All game binaries, assets, and trademarks are property of their respective owners (DivoGames / Game Insight / Alawar Entertainment / MyPlayCity). Do not use these tools to circumvent copy protection for commercial gain or to distribute copyrighted material without authorization.

---

## License

This repository is licensed under the [MIT License](license).

> *Because knowledge should be free, just like the joy of playing games.*

---

## Acknowledgments

To that old PC that could barely run the game but somehow made it magical anyway.

---

## Related Resources

- [r/airstrike3d](https://www.reddit.com/r/airstrike3d/) — community research & modding
- [Ithamar's APK scripts](https://gist.github.com/Ithamar/85f1f71d179c354fad483a8c48767daf) — updated extraction with text decryption
- [QindieGL](https://github.com/nicedrak/QindieGL) — OpenGL-to-D3D wrapper for running on modern Windows
- [PCGamingWiki: AirStrike 2](https://www.pcgamingwiki.com/wiki/AirStrike_2) — compatibility fixes
- [xakep.ru: ASProtect taming](https://xakep.ru/2003/07/10/19112/) — technical packer analysis (Russian)
- [ASProtect homepage](http://asprotect.net) — Alexey Solodovnikov's official site
