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
```

## Features

- **Runtime Performance Monitoring**: FPS, memory usage, system stats
- **Visual Effects**: Post-processing shaders (vignette, sepia, scanlines, invert)
- **Hook Analysis**: Real-time display of active function intercepts
- **Debug Controls**: Wireframe toggle, clear color, ImGui theming

## Technical Implementation

- **DLL Export Forwarding**: All BASS audio APIs transparently forwarded
- **OpenGL Interception**: Hooks `wglSwapBuffers` for frame capture
- **Framebuffer Objects**: Renders game to texture for post-processing
- **GLSL Shaders**: Hardware-accelerated visual effects
- **ImGui Integration**: Immediate-mode GUI overlay system

## Security Warnings

⚠️ **This is a code injection technique**:

- May trigger antivirus/anti-cheat detection
- Modifies system DLL behavior at runtime
- Can cause game instability if incorrectly implemented
- Educational/research use only

---

**Note**: This proxy requires the legitimate BASS audio library. Ensure you have proper licensing for any redistributed BASS components.