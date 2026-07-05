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
Lua plugin scripts (LuaBridge3)
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
- **Lua Scripting Engine**: LuaBridge3 backend (2.6× faster than sol2)

## Lua Scripting

The proxy uses [LuaBridge3](https://github.com/kunitoki/LuaBridge3) for C++/Lua bindings:

- Header-only, MIT-licensed
- 2.6× faster Lua→C++ calls compared to sol2
- Leaner compile times
- Raw `lua_State*` with `luabridge::LuaRef` for type-safe callbacks

See `sdk/scripting/detail/readme.md` for Lua API documentation.

## Directory Structure

```
src/proxy/
├── CMakeLists.txt              # Build configuration
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
    │   └── detail/             # LuaBridge3 implementation
    ├── ui/                     # ImGui wrapper functions
    └── sdk.hpp                 # Umbrella header
```

## Build Instructions

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Security Warnings

⚠️ **This is a code injection technique**:

- May trigger antivirus/anti-cheat detection
- Modifies system DLL behavior at runtime
- Can cause game instability if incorrectly implemented
- Educational/research use only

---

**Note**: This proxy requires the legitimate BASS audio library. Ensure you have proper licensing for any redistributed BASS components.
