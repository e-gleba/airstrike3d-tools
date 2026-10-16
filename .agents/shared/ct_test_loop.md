# Test loop (native vs cross)

Native (`msvc`, `clang_windows_x86` — Windows runners only):
1. `cmake --preset <preset>`
2. `cmake --build build/<preset> --config Debug`
3. `ctest --test-dir build/<preset> --output-on-failure`
4. Or one shot: `cmake --workflow --preset <preset>-debug-with-tests`

Cross (`llvm-mingw-i686` — Linux runners):
1. `cmake --preset llvm-mingw-i686`
2. `cmake --build build/llvm-mingw-i686 --config Debug`
3. `ctest --test-dir build/llvm-mingw-i686 --output-on-failure`
   (emulator tests SKIP without Steam + Proton; deploy + proxy unit tests still run)
4. Or one shot: `cmake --workflow --preset llvm-mingw-i686-debug-with-tests`

CI presets (`*-release`) skip the test step; `-with-tests` variants include it.
On failure: forbid the failed command, move one step closer to the
changed surface (preset JSON -> toolchain file -> `CMakeLists.txt`).

Never build Release to "just check". Debug-with-tests is the test config.
Packaging (ZIP native, TXZ cross) is release-only.
