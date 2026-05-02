For native Windows builds with **Clang** (pure GCC-style driver — not `clang-cl`):

```bash
cmake --preset clang_windows_x86
cmake --build --preset clang_windows_x86
```

> **Prerequisites for native Windows builds:**
> - LLVM/Clang in `PATH` (`clang`, `clang++`, `lld`, `llvm-rc`)
> - MSVC / Windows SDK installed (required for standard-library headers and import libraries)
> - Ninja
