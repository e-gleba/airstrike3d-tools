# AirStrike3D Source Code (`src/`)

This directory contains the C++ implementation of a **bass.dll proxy** for AirStrike 3D game modification and runtime analysis.

## What is a DLL Proxy?

A DLL proxy replaces the game's original `bass.dll` (BASS audio library) while forwarding all legitimate audio API calls to the renamed original library. This technique allows code injection without modifying the game executable directly.

## Architecture

```
Game Process
    ↓ loads bass.dll
Your Proxy DLL
    ↓ forwards BASS_* calls to
bass_real.dll (original BASS library)
    ↓ meanwhile injects
OpenGL hooks + ImGui overlay
    ↓ and runs
Lua plugin scripts (sol2 or LuaBridge3 backend)
```

## Features

- **Runtime Performance Monitoring**: FPS, memory usage, system stats
- **Visual Effects**: Post-processing shaders (vignette, sepia, scanlines, invert)
- **Hook Analysis**: Real-time display of active function intercepts
- **Debug Controls**: Wireframe toggle, clear color, ImGui theming
- **Lua Scripting**: Plugin system for modding and automation

## Technical Implementation

- **DLL Export Forwarding**: All BASS audio APIs transparently forwarded
- **OpenGL Interception**: Hooks `wglSwapBuffers` for frame capture
- **Framebuffer Objects**: Renders game to texture for post-processing
- **GLSL Shaders**: Hardware-accelerated visual effects
- **ImGui Integration**: Immediate-mode GUI overlay system
- **Lua Scripting Engine**: Pluggable backend (sol2 default, LuaBridge3 experimental)

## Lua Backend Selection

The proxy includes a Lua scripting engine for plugin development. Two backends are available:

### Default: sol2

- Rich API with extensive type safety
- Heavy compile times due to template metaprogramming
- Battle-tested in production

### Experimental: LuaBridge3

Enable with `-DSDK_EXPERIMENTAL_LUABRIDGE3=ON`:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DSDK_EXPERIMENTAL_LUABRIDGE3=ON
cmake --build build
```

- 2.6× faster Lua→C++ calls (benchmark: 85ms vs 225ms for Increment)
- Leaner compile times, linear scaling
- Header-only, MIT-licensed ([kunitoki/LuaBridge3](https://github.com/kunitoki/LuaBridge3))
- Same Lua API surface — no plugin changes needed

See `sdk/scripting/detail/readme.md` for full backend comparison and migration guide.

## Directory Structure

```
src/proxy/
├── CMakeLists.txt              # Build configuration with backend selection
├── dll_main.cpp                # DLL entry point and BASS forwarding
├── readme.md                   # This file
└── sdk/                        # Version-agnostic SDK library
    ├── core/                   # Core utilities, types, contracts
    ├── graphics/               # OpenGL bindings and constants
    ├── math/                   # Vector math utilities
    ├── overlay/                # ImGui overlay rendering
    ├── platform/               # OS-specific functions, logging
    ├── scripting/              # Lua scripting engine
    │   ├── callback.hpp        # Thread-safe callback lists
    │   ├── engine.hpp          # Public engine interface (pimpl)
    │   └── detail/             # Backend implementations (sol2/LuaBridge3)
    ├── ui/                     # ImGui wrapper functions
    └── sdk.hpp                 # Umbrella header
```

## Build Instructions

### Standard Build (sol2 backend)

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Experimental LuaBridge3 Backend

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DSDK_EXPERIMENTAL_LUABRIDGE3=ON
cmake --build build
```

### Run Tests

```bash
ctest --test-dir build --output-on-failure
```

## Security Warnings

⚠️ **This is a code injection technique**:

- May trigger antivirus/anti-cheat detection
- Modifies system DLL behavior at runtime
- Can cause game instability if incorrectly implemented
- Educational/research use only

---

**Note**: This proxy requires the legitimate BASS audio library. Ensure you have proper licensing for any redistributed BASS components.
