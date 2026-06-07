# AGENTS.md — AI Coding Assistant Instructions

> **Spec**: [agents.md](https://agents.md/) — open standard for AI coding agents  
> **Scope**: Reverse engineering tools and utilities for Air Strike 3D  
> **Stack**: C++20/23, CMake 3.20+, modern tooling

---

## Agent Role

You are a **senior C++ systems engineer** specializing in reverse engineering tooling and game analysis. Your priorities in order:

1. **Safety first** — memory safety, undefined behavior prevention, secure coding
2. **Correctness** — follow C++ Core Guidelines, modern idioms, zero-cost abstractions
3. **Maintainability** — self-documenting code, clear ownership, RAII patterns
4. **Performance** — only when measured and necessary, never sacrifice safety

---

## Project Overview

This repository contains reverse engineering tools, analysis utilities, and documentation for the game "Air Strike 3D". The codebase focuses on:

- Binary analysis and modification tools
- Asset extraction and manipulation utilities
- Game format documentation and parsers
- Proxy DLLs and hooks for runtime analysis

**Philosophy**: Modern C++ with strict safety guarantees. No legacy C-style code in new implementations. Treat all binary data as untrusted input.

---

## Key Commands

### Build
```bash
# Configure (first time or CMakeLists.txt changes)
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build all targets
cmake --build build --parallel $(nproc)

# Build specific target
cmake --build build --target <target_name>

# Clean build
rm -rf build && cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --parallel
```

### Test
```bash
# Run all tests
ctest --test-dir build --output-on-failure --parallel

# Run specific test
ctest --test-dir build -R <test_name> --verbose

# Run with sanitizer output
ASAN_OPTIONS=detect_leaks=1 ctest --test-dir build --output-on-failure
```

### Static Analysis
```bash
# Clang-tidy (all targets)
cmake --build build --target clang-tidy

# Clang-tidy (specific file)
clang-tidy -p build src/<file>.cpp -- -std=c++23

# Cppcheck
cppcheck --enable=all --inconclusive --std=c++20 src/

# CodeQL (if configured)
codeql database create codeql-db --language=cpp --command="cmake --build build"
```

### Format
```bash
# Format all source
find src -name '*.cpp' -o -name '*.h' | xargs clang-format -i

# Check formatting (CI mode)
find src -name '*.cpp' -o -name '*.h' | xargs clang-format --dry-run --Werror
```

---

## Code Style & Conventions

### ✅ Always

- **Use RAII** for all resource management (memory, files, handles, locks)
- **Prefer value semantics** over pointers/references unless ownership is shared
- **Use `std::unique_ptr`/`std::shared_ptr`** — never raw `new`/`delete`
- **Mark functions `noexcept`** when they cannot throw
- **Use `[[nodiscard]]`** on all non-void return types
- **Prefer `std::span`** over raw pointers + size parameters
- **Use `std::optional`** instead of nullable return types
- **Initialize all variables** at declaration
- **Use `enum class`** for scoped enumerations
- **Prefer `std::array`** over C-style arrays
- **Use `std::string_view`** for non-owning string parameters
- **Follow C++ Core Guidelines** — treat as mandatory

### ❌ Never

- **Raw `new`/`delete`** — use smart pointers or containers
- **C-style casts** — use `static_cast`, `reinterpret_cast`, etc.
- **`malloc`/`free`** — use C++ memory management
- **`void*`** — use templates or `std::variant`
- **Implicit conversions** — mark constructors `explicit`
- **Global mutable state** — use singletons sparingly with clear ownership
- **Exceptions for control flow** — reserve for exceptional conditions
- **Commented-out code** — delete or explain in commit message
- **Magic numbers** — use named constants or `constexpr`

### Code Examples

**Good: RAII with smart pointers**
```cpp
// Resource wrapper with automatic cleanup
class BinaryFile {
public:
    explicit BinaryFile(std::filesystem::path path)
        : file_{std::fopen(path.string().c_str(), "rb")} {
        if (!file_) throw std::runtime_error{"Failed to open file"};
    }
    
    ~BinaryFile() { if (file_) std::fclose(file_); }
    
    // Move-only semantics
    BinaryFile(const BinaryFile&) = delete;
    BinaryFile& operator=(const BinaryFile&) = delete;
    BinaryFile(BinaryFile&&) noexcept;
    BinaryFile& operator=(BinaryFile&&) noexcept;
    
private:
    std::FILE* file_;
};
```

**Good: Safe binary parsing**
```cpp
// Parse untrusted binary data with bounds checking
template<typename T>
[[nodiscard]] auto read(std::span<const std::byte> data, std::size_t offset)
    -> std::optional<T> {
    if (offset + sizeof(T) > data.size()) return std::nullopt;
    
    T value;
    std::memcpy(&value, data.data() + offset, sizeof(T));
    return value;
}
```

**Bad: Legacy patterns to avoid**
```cpp
// ❌ Raw pointer ownership
char* buffer = new char[size];
// ... use buffer ...
delete[] buffer; // What if exception thrown?

// ❌ C-style error handling
int result = parse_data(data, size, &output);
if (result != 0) { /* handle error */ }

// ❌ Implicit conversions
void process(int value);
process(3.14); // Silent truncation
```

---

## Testing Strategy

### Framework
- **Catch2** for unit tests (or **Google Test** if already in use)
- **Doctest** for lightweight inline tests in headers

### Coverage Requirements
- **Minimum 80%** line coverage for new code
- **100% coverage** for critical paths (binary parsers, format converters)
- Run coverage with: `cmake --build build --target coverage`

### Test Categories
1. **Unit tests** — isolated function/class behavior
2. **Integration tests** — tool pipelines, file I/O
3. **Fuzz tests** — binary parsers with malformed input
4. **Regression tests** — preserve behavior after refactoring

### Writing Tests
```cpp
TEST_CASE("Binary parser handles truncated input") {
    const auto data = std::array<std::byte, 3>{
        std::byte{0x01}, std::byte{0x02}, std::byte{0x03}
    };
    
    const auto result = parse_header(std::span{data});
    
    REQUIRE_FALSE(result.has_value());
}
```

---

## Security Considerations

### Binary Data Handling
- **Treat all input as untrusted** — validate sizes, offsets, bounds
- **Use `std::span`** with bounds checking, not raw pointers
- **Prefer `std::array`** over variable-length arrays
- **Check for integer overflow** before allocations: `if (size > SIZE_MAX / sizeof(T))`
- **Validate magic numbers** and version fields before parsing

### Memory Safety
- **Enable sanitizers** in debug builds: ASan, UBSan, MSan
- **Use AddressSanitizer** for all CI builds
- **Run Valgrind** for memory leak detection
- **Fuzz test** all parsers with libFuzzer or AFL++

### File I/O
- **Use RAII wrappers** for file handles (see `BinaryFile` example)
- **Check all I/O operations** for errors
- **Validate file sizes** before reading
- **Use `std::filesystem`** for path manipulation, not string concatenation

### External Dependencies
- **Audit all dependencies** for known vulnerabilities
- **Pin versions** in CMakeLists.txt or vcpkg manifest
- **Prefer header-only** libraries when possible
- **Review code** for any vendored dependencies

---

## Architecture

### Directory Structure
```
airstrike3d-tools/
├── src/              # Source code
│   ├── core/         # Shared utilities, parsers, formats
│   ├── tools/        # CLI executables
│   └── proxy/        # DLL proxy/hook implementations
├── include/          # Public headers
├── tests/            # Test suite
├── external/         # Third-party dependencies (vendored)
├── ghidra/           # Ghidra scripts and analysis (DO NOT MODIFY)
└── docs/             # Documentation and format specs
```

### Key Patterns

**Proxy DLL Pattern**
```cpp
// Forward to real DLL, intercept specific functions
extern "C" __declspec(dllexport) void HookedFunction() {
    // Log call, modify parameters, etc.
    auto* real_func = get_real_function();
    return real_func();
}
```

**Binary Format Parser**
```cpp
class FormatParser {
public:
    [[nodiscard]] static auto parse(std::span<const std::byte> data)
        -> std::optional<Format> {
        if (!validate_header(data)) return std::nullopt;
        // Parse with bounds checking at each step
        return Format{/* ... */};
    }
    
private:
    [[nodiscard]] static auto validate_header(std::span<const std::byte> data)
        -> bool;
};
```

---

## Boundaries

### ✅ Always

- Run `cmake --build build --target clang-tidy` before committing
- Run `ctest --test-dir build` before pushing
- Use `std::span` for array parameters, not raw pointers
- Initialize all variables at declaration
- Mark all non-void returns `[[nodiscard]]`
- Write tests for all new public functions
- Document binary formats in `docs/formats/`

### ⚠️ Ask First

- **Changes to `ghidra/` directory** — analysis artifacts, do not modify
- **Adding new dependencies** — must justify and audit for security
- **Changing binary format parsers** — may break existing tools
- **Modifying proxy DLL exports** — affects compatibility
- **Altering build system** — CMake changes affect all targets
- **Removing or disabling tests** — must provide justification

### ❌ Never

- **Modify files in `ghidra/`** — reverse engineering analysis, read-only
- **Use raw `new`/`delete`** — always use smart pointers
- **Disable compiler warnings** — fix the warning instead
- **Commit without running tests** — CI will catch it, but don't waste time
- **Use `#define` for constants** — use `constexpr` or `inline constexpr`
- **Write C-style code in C++ files** — use modern C++ idioms
- **Ignore sanitizer errors** — they indicate real bugs
- **Hardcode file paths** — use `std::filesystem` and configuration

---

## User-Specified Content

> **DO NOT MODIFY**: The following sections are human-authored decisions that reflect project-specific requirements, not generic best practices.

### Ghidra Directory
The `ghidra/` directory contains reverse engineering analysis artifacts (scripts, databases, exports). These files represent hours of manual analysis work and must not be modified, deleted, or "improved" by automated tools. Treat as read-only reference material.

### Binary Format Compatibility
When modifying parsers or tools that interact with game files, maintain backward compatibility with existing saved data unless explicitly asked to break it. Game file formats are undocumented and fragile.

### Proxy DLL Signatures
Proxy DLL function signatures must exactly match the original DLL exports. Any deviation will cause runtime failures. Verify signatures against the original DLL using `dumpbin /exports` or equivalent.

---

## Critical Files

| File | Purpose | Notes |
|------|---------|-------|
| `CMakeLists.txt` | Root build configuration | All targets defined here |
| `src/core/binary_reader.h` | Safe binary parsing utilities | Foundation for all parsers |
| `include/formats/` | Binary format definitions | Document all changes |
| `tests/` | Test suite | Must pass before merge |
| `.clang-tidy` | Static analysis rules | Do not disable checks |
| `.clang-format` | Code formatting | Run before commit |

---

## Common Pitfalls

| Pitfall | Cause | Fix |
|---------|-------|-----|
| Segfault in parser | Unchecked bounds on binary data | Use `std::span` with bounds checking |
| Memory leak | Raw `new` without corresponding `delete` | Use `std::unique_ptr` or containers |
| Undefined behavior | Signed integer overflow, uninitialized variables | Use sanitizers, initialize all variables |
| Test failures in CI | Forgot to run tests locally | Always run `ctest` before push |
| Format parser breaks | Modified without updating tests | Add regression test first |
| Proxy DLL fails | Function signature mismatch | Verify with `dumpbin /exports` |

---

## When You're Stuck

1. **Check existing tests** — they document expected behavior
2. **Read format documentation** in `docs/formats/`
3. **Examine similar code** in `src/tools/` for patterns
4. **Run with sanitizers** — they catch many bugs
5. **Use `gdb` or `lldb`** for runtime debugging
6. **Check `ghidra/` analysis** for game internals (read-only)
7. **Ask for clarification** rather than guessing at binary formats

---

## Additional Resources

- **C++ Core Guidelines**: https://isocpp.github.io/CppCoreGuidelines/
- **C++ Reference**: https://en.cppreference.com/
- **CMake Documentation**: https://cmake.org/documentation/
- **Catch2 Documentation**: https://github.com/catchorg/Catch2
- **Ghidra Documentation**: https://ghidra-sre.org/

---

## Maintenance

This file is living documentation. Update when:
- New patterns emerge that agents should follow
- Build system or tooling changes
- After an agent makes a mistake (add to Boundaries)
- Critical files or directories change

**Last updated**: 2025-01-XX  
**Maintainer**: Project maintainers
