# Contributing to AirStrike 3D — Reverse Engineering Toolkit

Thank you for your interest in contributing! This project is a community-driven effort to preserve and understand the AirStrike 3D engine. Whether you are submitting a bug fix, a new tool, additional reverse engineering notes, or documentation improvements, your contribution is valued.

## Code of Conduct

Be respectful, constructive, and collaborative. Harassment or toxic behavior will not be tolerated.

## How to Contribute

1. **Fork the repository** and create a feature branch.
2. **Make your changes** following the guidelines below.
3. **Test your changes** locally.
4. **Submit a pull request** using the provided PR template.

### Reporting Issues

- Use the [Bug Report](https://github.com/e-gleba/airstrike3d-tools/issues/new?template=bug_report.md) template for crashes, incorrect behavior, or documentation errors.
- Use the [Feature Request](https://github.com/e-gleba/airstrike3d-tools/issues/new?template=feature_request.md) template for new tools, format support, or enhancements.
- For security-sensitive matters, see [`SECURITY.md`](SECURITY.md).

## Development Guidelines

### C++ Code

- Follow the existing `.clang-format` and `.clang-tidy` configurations.
- The project targets **C++26** with extensions disabled.
- Use the `warnings` CMake target for consistent compiler flags.
- Prefer explicit types and clear naming; the codebase is reverse-engineered, so clarity is paramount.

### Python Scripts

- Compatible with **Python 3.x**.
- Follow PEP 8 where practical.
- Include docstrings for public functions.
- Add CLI help text for new scripts or commands.

### CMake & Build

- Minimum CMake version: **3.31**.
- Use the provided `CMakePresets.json` for reproducible builds.
- Do not hard-code absolute paths; use `${CMAKE_SOURCE_DIR}` and `CMAKE_PREFIX_PATH`.

### Documentation

- Update `README.md` if your change affects user-facing behavior, build steps, or supported formats.
- Keep the Mermaid diagram in sync if you modify engine version or game relationships.
- Use American English for consistency.

## Commit Message Style

Use clear, descriptive commit messages:

```
<area>: <what changed> (<why>)

- Detail 1
- Detail 2
```

Examples:

```
scripts: add v2.50 pak format decryption
docs: update engine internals with D3D caps
cmake: add MSVC ARM64 preset
```

## Areas Needing Help

- Confirming asset format compatibility for **Air Force Missions** and **Space Strike**
- Additional Ghidra decompilation annotations for `G_LoadBin` and `R_LoadModel`
- ImHex patterns for `.mdl` and `.mo3` headers
- Cross-platform audio playback tooling (MO3 → modern formats without `openmpt123` dependency)

## Questions?

Open a [discussion](https://github.com/e-gleba/airstrike3d-tools/discussions) (if enabled) or reach out via the related community channels listed in the README.

## License

By contributing, you agree that your contributions will be licensed under the same [MIT License](license) as the project.
