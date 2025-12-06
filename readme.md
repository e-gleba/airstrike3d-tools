# euengine

A modern, cross-platform game engine built with C++26, SDL3, and Vulkan. Designed for rapid game development with hot-reloadable game modules, ECS architecture, and a clean separation between engine and game code.

## Features

- **SDL3 Callback Architecture**: Modern SDL3 main loop with callback-based event handling
- **Hot-Reloadable Games**: Load and reload game modules at runtime without restarting the engine
- **ECS System**: Entity Component System using EnTT for flexible game object management
- **Vulkan Rendering**: High-performance GPU rendering via SDL3 GPU abstraction
- **Audio System**: Music and sound effects support via SDL3_mixer
- **ImGui Integration**: Built-in debug UI and editor tools
- **Model Loading**: Support for OBJ and glTF/GLB formats
- **Shader Hot-Reload**: Live shader editing during development
- **Cross-Platform**: Windows, Linux, and macOS support

## Architecture

### Engine Core

The engine is split into two main parts:

- **Core API** (`src/engine/core-api/`): Public interfaces for games to use
    - `game.hpp` - Game callback definitions and engine context
    - `engine.hpp` - Engine settings interface
    - `renderer.hpp` - Rendering interface
    - `audio.hpp` - Audio interface
    - `window.hpp` - Window and display settings
    - `camera.hpp` - Camera component for ECS

- **Engine Implementation** (`src/engine/private/`): Internal engine implementation
    - SDL3 integration
    - GPU device management
    - Subsystem initialization
    - Game library hot-loading

### Game Module

Games are compiled as shared libraries that export standard callbacks:

- `game_preinit()` - Configure engine settings before initialization (optional)
- `game_init()` - Initialize game state
- `game_update()` - Update game logic each frame
- `game_render()` - Render game objects
- `game_ui()` - Render ImGui UI
- `game_shutdown()` - Cleanup game resources

## Building

### Prerequisites

- CMake 3.30 or later
- C++26 compatible compiler (Clang recommended)
- SDL3 development libraries
- Vulkan SDK

### Build Instructions

1. **Configure the project**:

```bash
cmake --preset clang
```

2. **Build the engine and game**:

```bash
cmake --build --preset clang --target engine --target game
```

3. **Run the engine**:

```bash
./build/clang/src/engine/private/engine
```

### Build Presets

The project includes several CMake presets:

- **clang**: Native build with Clang (Linux/macOS)
- **llvm-mingw-x86_64**: Cross-compile for Windows 64-bit

To use a different preset:

```bash
cmake --preset <preset-name>
cmake --build --preset <preset-name>
```

## Project Structure

```
airstrike3d-tools/
├── src/
│   ├── engine/
│   │   ├── core-api/          # Public API headers
│   │   └── private/            # Engine implementation
│   └── game/                   # Game module source
├── assets/                     # Game assets
│   ├── models/                 # 3D models
│   ├── gfx/                    # Textures and graphics
│   ├── music/                  # Music tracks
│   └── sounds/                 # Sound effects
├── shaders/                    # GPU shaders
├── cmake/                      # CMake configuration
└── build/                      # Build output (gitignored)
```

## Usage

### Creating a Game

1. **Implement the game callbacks** in your game module:

```cpp
#include <core-api/game.hpp>

// Optional: Configure engine before initialization
GAME_API euengine::preinit_result game_preinit(
    euengine::preinit_settings* settings)
{
    settings->window.width  = 1920;
    settings->window.height = 1080;
    settings->window.title  = "My Game";
    return euengine::preinit_result::ok;
}

// Initialize game state
GAME_API bool game_init(euengine::engine_context* ctx)
{
    // Create entities, load resources, etc.
    return true;
}

// Update game logic
GAME_API void game_update(euengine::engine_context* ctx)
{
    // Update entities, handle input, etc.
}

// Render game objects
GAME_API void game_render(euengine::engine_context* ctx)
{
    // Draw models, meshes, etc.
}

// Render UI
GAME_API void game_ui(euengine::engine_context* ctx)
{
    // ImGui UI code
}

// Cleanup
GAME_API void game_shutdown()
{
    // Unload resources
}
```

2. **Compile as a shared library** and place it next to the engine executable

3. **Run the engine** - it will automatically load your game module

### Hot Reload

Press **F5** during runtime to reload the game module without restarting the engine. This is useful for rapid iteration during development.

### Controls

- **WASD** - Move camera
- **QE** - Move camera up/down
- **Shift** - Fast movement
- **Mouse** - Look around (click to capture)
- **ESC** - Release mouse
- **F5** - Hot reload game
- **F11** - Toggle fullscreen
- **Tab** - Toggle wireframe mode

## Dependencies

- **SDL3** - Window management, events, and platform abstraction
- **SDL3_gpu** - GPU rendering abstraction
- **SDL3_mixer** - Audio playback
- **EnTT** - Entity Component System
- **GLM** - Mathematics library
- **ImGui** - Immediate mode GUI
- **spdlog** - Logging
- **tinyobjloader** - OBJ model loading
- **tinygltf** - glTF/GLB model loading
- **yaml-cpp** - YAML configuration parsing
- **stb** - Image loading utilities

## Development

### Code Style

The project follows a strict style guide (see `.cursor/rules/styling.mdc`):

- **snake_case** for all identifiers (including classes)
- **Boost/C++ Core Guidelines** best practices
- **Explicit error handling** - no silent failures
- **RAII** for all resource management
- **Minimal API surface** - prefer free functions over classes
- **Standard library** over custom implementations

### Logging

The engine uses `spdlog` for logging. Log levels:

- `spdlog::info()` - General information
- `spdlog::warn()` - Warnings
- `spdlog::error()` - Errors
- `spdlog::debug()` - Debug information

### Shader Development

Shaders are located in `shaders/` and support hot-reload during development. The engine automatically watches for changes and reloads shaders when modified.

## License

MIT License - see [license](license) file for details.

Copyright (c) 2025 Eugene

## Contributing

This is a personal project, but suggestions and improvements are welcome. Please follow the project's code style and ensure all changes compile without warnings.

## Acknowledgments

- SDL3 team for the excellent cross-platform library
- EnTT for the fast ECS implementation
- Casey Muratori for design philosophy inspiration