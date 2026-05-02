# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Professional repository structure: `CONTRIBUTING.md`, `SECURITY.md`, `CHANGELOG.md`, issue templates, and PR template.
- Expanded `README.md` with badges, table of contents, project status dashboard, and repository structure tree.

## [1.0.6] - 2025-??-??

### Added
- CMake-based C++ build system with LLVM-MinGW and MSVC presets.
- BASS proxy DLL (`src/proxy/`) for runtime injection and ImGui overlay.
- Decompiled game logic stubs (`src/game/2_06.c`, `src/game/2_71.c`).
- Python tooling suite:
  - `paktool.py` — archive extraction and repacking
  - `mdl_obj_converter.py` — model format converter
  - `save_editor.py` — save file decryption and preview
  - `level_viewer.py` — level data visualization
  - `static_exe_unpacker.py` — ASProtect 1.0 static unpacker
- Ghidra project files for both engine versions (`ghidra/as3d2`, `ghidra/as3d-gulf`).
- Game binaries and sample data for v2.06 and v2.71 (`2_06/`, `2_71/`).
- Code quality configuration (`.clang-format`, `.clang-tidy`, `.editorconfig`, `.cmake-format.py`).

### Notes
- This release marks the initial public tooling release for the AirStrike 3D reverse engineering effort.
