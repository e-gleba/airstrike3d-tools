# AirStrike 3D II: Gulf Thunder — Binary Analysis

> PE analysis of `AirStrike3D II - Gulf.exe` from the [airstrike3d-tools](https://github.com/e-gleba/airstrike3d-tools) repository.
> Unlike the ASProtect-packed v2.06, this binary ships **completely unprotected** — making it the best target for engine research.

## file info

| Field | Value |
|-------|-------|
| Filename | `AirStrike3D II - Gulf.exe` |
| Size | 655,360 bytes (640 KB) |
| MD5 | `33a194c818dd6c87e5f7664702d51f92` |
| SHA256 | `86195a9653489064844c172ce43307c703a50e53be7e00d45fe346c45d5ae077` |
| Machine | `0x14C` (i386) |
| Compile date | `2007-05-15 13:49:28 UTC` (`0x4649BA68`) |
| Linker | 8.0 (MSVC 8.0 / Visual Studio 2005) |
| Image base | `0x00400000` |
| Entry point | `0x0003A076` (VA `0x0043A076`) — in `.text` section |
| EP bytes | `E8 B8 8F 00 00 E9 16 FE FF FF ...` |
| Size of image | `0x1E1C000` (~30.5 MB virtual) |
| Subsystem | `2` (Windows GUI) |
| Checksum | `0x0` (not set) |
| Protection | **NONE** |
| Window title | `Airstrike II: Gulf - Divo Games` |

## developers

Embedded as string literals in the executable credits data:

```
{Anton Petrov}
{Dmitry Zakharov}
DivoGames
Divo Master
```

- **Anton Petrov** — engine architect, CTO & co-founder ([LinkedIn](https://www.linkedin.com/in/anton-petrov-cto/))
- **Dmitry Zakharov** — co-founder

## rich header (toolchain)

Rich header confirms **Visual Studio 2005 RTM** as the primary toolchain, with some legacy VS6-era object files linked in (likely DirectX SDK libs).

| Tool | Build | Version |
|------|-------|---------|
| `cl.exe` (C/C++ compiler) | 50727 | MSVC 14.00.50727 (VS2005 RTM) |
| `link.exe` (linker) | 50727 | 8.00.50727 |
| `cvtres.exe` | 50727 | 8.00.50727 |
| `ml.exe` (MASM) | 9178 | 6.15.9178 |
| Legacy objects | 8444, 8447 | VS6 SP5/SP6 era (DirectX SDK?) |
| SDK resource compiler | 4035 | Old platform SDK |

55 C/C++ compilation units, 155 C compilation units, 80 linker-generated entries.

## sections

| Name | VA | VirtSize | RawSize | Entropy | Flags |
|------|----|----------|---------|---------|-------|
| `.text` | `0x1000` | 509,893 | 512,000 | 6.83 | `CODE\|EXECUTE\|READ` |
| `.rdata` | `0x7E000` | 94,304 | 98,304 | 6.04 | `INITIALIZED_DATA\|READ` |
| `.data` | `0x96000` | 30,942,628 | 28,672 | 4.30 | `INITIALIZED_DATA\|READ\|WRITE` |
| `.data1` | `0x1E19000` | 2,304 | 4,096 | 2.51 | `INITIALIZED_DATA\|READ\|WRITE` |
| `.rsrc` | `0x1E1A000` | 4,872 | 8,192 | 3.30 | `INITIALIZED_DATA\|READ` |

`.text` entropy 6.83 = normal compiled code (not packed). The massive `.data` virtual size (30 MB) with only 28 KB raw is BSS/zero-initialized runtime memory — the game allocates its world data there.

## imports (178 functions, 9 DLLs)

### d3d8.dll (1)

```
Direct3DCreate8
```

Single import — the engine calls all other D3D8 methods through the returned `IDirect3D8` vtable. This is Direct3D 8 (not 9).

### BASS.dll (28)

Full 3D positional audio with EAX support:

```
BASS_Init, BASS_Free, BASS_Start, BASS_Stop, BASS_Pause, BASS_Update
BASS_MusicLoad, BASS_MusicFree, BASS_MusicGetLength
BASS_SampleLoad, BASS_SampleFree, BASS_SampleGetChannel
BASS_SampleGetInfo, BASS_SampleSetInfo
BASS_ChannelPlay, BASS_ChannelStop, BASS_ChannelIsActive
BASS_ChannelSetPosition, BASS_ChannelSetAttributes
BASS_ChannelSetFlags, BASS_ChannelGetInfo
BASS_ChannelSet3DPosition
BASS_Set3DPosition, BASS_Set3DFactors, BASS_Apply3D
BASS_SetEAXParameters, BASS_SetConfig, BASS_GetVersion
```

### WININET.dll (6)

Online score posting system ("DivoRating"):

```
InternetAttemptConnect, InternetOpenA, InternetConnectA
HttpOpenRequestA, HttpSendRequestA, InternetCloseHandle
```

User-agent: `Mozilla/4.0 (compatible; MSIE 5.0; Windows 98)`

### WINMM.dll (6)

Joystick input + high-resolution timer:

```
joyGetNumDevs, joyGetDevCapsA, joyGetPosEx
timeBeginPeriod, timeEndPeriod, timeGetTime
```

### KERNEL32.dll (93), USER32.dll (37), GDI32.dll (1), ADVAPI32.dll (5), SHELL32.dll (1)

Standard Win32 APIs. Notable: `IsDebuggerPresent` is imported but only as part of CRT exception handling (not anti-debug). Registry access for D3D settings and game registration (`Software\CLASSES\CLSID\{72C33BB5-7A28-4E74-B085-AF9FE31F58B7}\ProgID`). Config via `GetPrivateProfileStringA` / `WritePrivateProfileStringA` (`config.ini`).

## engine architecture

### subsystem prefixes (quake-style)

| Prefix | Subsystem | Functions found |
|--------|-----------|-----------------|
| `G_` | Game logic | `G_LoadBin`, `G_LoadLevelList`, `G_LoadObjects`, `G_AddMissiles`, `G_AddPowerUp`, `G_GetMissiles`, `G_GetPowerUp`, `G_GetUpgrade`, `G_SetPowerUpCount`, `G_SetUpgrade`, `G_UseMissile`, `G_UsePowerUp` |
| `R_` | Renderer | `R_LoadModel`, `R_RegisterModel`, `R_RegisterShadow` |
| `S_` | Sound | `S_Init`, `S_RegisterSound` |
| `F_` | File system | `F_LoadPackFile` |
| `MW_` | Window | `MW_CreateWindow` |

### init sequence (from embedded log strings)

```
---- Initializing file system ----
---- Initializing main window ----
--- Enumerating display modes ---
------ Direct3D Initializing ------
...creating Direct3D object: succeeded.
...registered window class.
...window creation: succeeded.
---- Initializing sound system ----
    BASS_Init: Succeeded.
```

### rtti class hierarchy

```
std::exception
  std::bad_exception
  std::bad_alloc
  std::logic_error
    std::length_error
    std::out_of_range
type_info

Utility::XmlNode
Utility::XmlNodeImpl  (XmlObject<XmlNodeImpl>)
Utility::XmlAttribute
Utility::XmlAttributeImpl  (XmlObject<XmlAttributeImpl>)

IntroPage
IntroPageImage
IntroPageDivoGames
```

Custom `Utility::` XML parser used for object definition files (`objects.txt`). Intro page class hierarchy handles splash screens.

### entity flags

```
FL_ONGROUND
FL_ONGROUND_NORMAL
FL_ONWATER
FL_ONWATER_FLAT
FL_ONWATER_NORMAL
```

Terrain system distinguishes ground vs water surfaces with normal-mapped variants.

### input action map

```
KeyMoveForward    KeyMoveBackward    KeyMoveLeft    KeyMoveRight
KeyPrimaryAttack  KeyMissileAttack
KeySwitchWeapon   KeySwitchMissiles  KeySwitchPowerUp
KeyUsePowerUp     KeyUsePowerup
```

Configurable via `Configure Controls` dialog. Joystick supported via WinMM.

### cheat system

```
God Mode: Enabled / Disabled
All Weapons: Enabled
All Missiles: Enabled
All Power-Ups: Enabled
All Lives: Enabled
sounds\cheat.wav        ← cheat activation sound
Cheater                 ← label applied when cheats used
```

### virtual file system

```
data\*.apk              ← wildcard scan for all .apk archives at startup
data\%s\*.%s            ← per-level asset loading pattern
F_LoadPackFile: Too many paks.
```

### online features (DivoRating)

```
PostScores / PostScore: Succeeded.
p_scores / score_num
===== Posting score =====
```

Score posting via HTTP to DivoGames servers. Custom CLSID `{72C33BB5-7A28-4E74-B085-AF9FE31F58B7}` used for registration/COM identity.

## asset formats

| Format | Path pattern | Description |
|--------|-------------|-------------|
| `.apk` | `data\*.apk` | Encrypted archives (XOR, 1024-byte key table) |
| `.mdl` | via `R_LoadModel` | Custom 3D models (versioned: header, vertices, normals, UVs, faces) |
| `.tga` | `gfx\**\*.tga`, `menu\*.tga`, `tiles\*.tga` | Textures (36 references) |
| `.wav` | `sounds\*.wav` | Sound effects (cheat.wav, menu1/2.wav, type.wav) |
| `.mo3` | via `BASS_MusicLoad` | Tracker music modules |
| `.txt` | `maps\levels.txt`, `objects.txt` | Level list, object definitions |
| `.ini` | `config.ini` | Game settings (Win32 PrivateProfile API) |
| `.bin` | `game.bin` | Save file (binary, encrypted) |

### known asset paths

```
gfx\logo\logo_gulf.tga          gfx\logo\clouds.tga
gfx\logo\glow.tga               gfx\logo\two3.tga
gfx\logo\lines_gulf.tga         gfx\lightning2.tga
gfx\mc_cur.tga                  gfx\logo.tga

gfx\ui\interface_gulf.tga        gfx\ui\mainbar2.tga
gfx\ui\life.tga                  gfx\ui\weapons.tga
gfx\ui\missiles.tga              gfx\ui\items.tga
gfx\ui\portraits2.tga            gfx\ui\font.tga
gfx\ui\font_alpha.tga            gfx\ui\grid.tga
gfx\ui\helicna.tga               gfx\ui\snow.tga

gfx\ui\comix\gameover_{1,2,3}_{1,2}.tga
gfx\ui\comix\gamov.tga
gfx\ui\comix\loading1_{0,1}_{0,1,2,3}

menu\cursor_1.tga                menu\cursor_2.tga
menu\controlsh_{0,1,2}.tga

tiles\tiles%i.tga
%s\texture%i.tga
%s\detail.tga

sounds\cheat.wav  sounds\menu1.wav  sounds\menu2.wav  sounds\type.wav
maps\levels.txt
```

### model format validation

`R_LoadModel` performs strict validation:

```
R_LoadModel ('%s'): File header corrupted.
R_LoadModel ('%s'): Illegal model version.
R_LoadModel ('%s'): Vertices are missing.
R_LoadModel ('%s'): Normals are missing.
R_LoadModel ('%s'): Texture coordinates are missing.
R_LoadModel ('%s'): Faces are missing.
R_LoadModel: Couldn't open model file '%s'.
R_RegisterModel: Too many registered models.
R_RegisterShadow: Too many shadowmaps.
```

MDL format stores: header → vertices → normals → UVs → faces, with a version field in the header.

## third-party libraries

| Library | Version evidence | Usage |
|---------|-----------------|-------|
| **BASS** (un4seen) | `BASS_GetVersion` import, version check at init | Audio: 3D positional, EAX, tracker modules |
| **libjpeg** | `Copyright (C) 1996, Thomas G. Lane` | JPEG texture loading |
| **zlib** | `zlib version error` | Decompression |
| **libpng** | `Incompatible libpng version`, `Not a PNG file` | PNG texture loading |
| **MSVC CRT** | `Microsoft Visual C++ Runtime Library` | Standard C/C++ runtime |

## gameplay features (from strings)

- **3 helicopters** — `Choose Helicopter`, `New helicopter is available.`
- **24 levels across 5 environments** — `@ 24 large levels across 5 environments`
- **Cooperative multiplayer** — `Player 1`, `Player 2`, `IsMultiplayer`, `Cooperative`
- **Weapon pages** — `PRIMARY WEAPONS (Page 1 of 2)`, `MISSILES (Page 1 of 2)`
- **DivoRating** — online leaderboard, `Post Scores`, `share high-scores all over the world`
- **Save file** — `game.bin`, `// This file was generated by Air Strike 3D II.`

## data directories

| Directory | VA | Size |
|-----------|-----|------|
| IMPORT | `0x00093F98` | `0xC8` |
| RESOURCE | `0x01E1A000` | `0x1308` |
| IAT | `0x0007E000` | `0x2EC` |

No EXPORT, no DEBUG, no TLS, no BASERELOC — a straightforward 32-bit GUI application.

## comparison with v2.06

| | v2.06 (`as3d2.exe`) | v2.71 (`Gulf.exe`) |
|---|---|---|
| **Protection** | ASProtect 1.0 | None |
| **Graphics** | OpenGL 1.1 | Direct3D 8 |
| **Compiler** | MSVC 7.0 (.NET 2002/2003) | MSVC 8.0 (VS2005) |
| **Compiled** | 2004-05-15 | 2007-05-15 |
| **Size** | 199 KB (packed) | 640 KB |
| **Visible imports** | 3 (packer stub) | 178 (full IAT) |
| **Strings** | Encrypted | All plaintext |
| **.text entropy** | 8.00 (encrypted) | 6.83 (normal) |
| **Developer names** | Hidden | `{Anton Petrov}`, `{Dmitry Zakharov}` |

## links

- [airstrike3d-tools](https://github.com/e-gleba/airstrike3d-tools) — reverse engineering toolkit (this repo)
- [r/airstrike3d](https://www.reddit.com/r/airstrike3d/) — community research & modding
- [PCGamingWiki: AirStrike 2](https://www.pcgamingwiki.com/wiki/AirStrike_2) — compatibility fixes
- [QindieGL](https://github.com/nicedrak/QindieGL) — OpenGL-to-D3D wrapper (for v2.06 on modern Windows)
- [Anton Petrov (LinkedIn)](https://www.linkedin.com/in/anton-petrov-cto/) — engine author, DivoGames CTO