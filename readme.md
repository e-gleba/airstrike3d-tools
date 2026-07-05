**Using presets (recommended for local testing):**

```bash
# Workflow preset WITH tests (requires Steam + Proton installed)
cmake --workflow --preset llvm-mingw-i686-release-with-tests

# Or run test preset directly after configure + build
ctest --preset llvm-mingw-i686-test-release
```

> **Note:** Standard CI workflow presets (`llvm-mingw-i686-release`, `clang_windows_x86-release`, `msvc-release`) do NOT include tests to avoid requiring Steam/Proton on CI runners. Use the `-with-tests` variants for local development or dedicated test environments.