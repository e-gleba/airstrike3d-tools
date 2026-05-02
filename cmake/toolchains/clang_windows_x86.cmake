# cmake/toolchains/clang_windows_x86.cmake
#
# Native Windows build: pure Clang (GCC-style driver) targeting 32-bit x86.
# Uses the MSVC ABI (Windows SDK + MSVC STL headers/libs) but compiles with
# the GCC-style clang driver (clang / clang++) — not clang-cl — and links
# with LLD.
#
# Host requirements:
#   - LLVM/Clang in PATH  (clang, clang++, lld, llvm-rc)
#   - MSVC/Windows SDK    (C++ standard library + import libraries)
#   - Ninja

include_guard(GLOBAL)

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR i686)

# ── compilers (GCC-style driver, NOT clang-cl) ────────────────────────────────
set(CMAKE_C_COMPILER clang CACHE FILEPATH "" FORCE)
set(CMAKE_CXX_COMPILER clang++ CACHE FILEPATH "" FORCE)

# Pass --target during compiler detection so try_compile() uses the right arch.
set(CMAKE_C_COMPILER_TARGET i686-pc-windows-msvc)
set(CMAKE_CXX_COMPILER_TARGET i686-pc-windows-msvc)

# ── linker ───────────────────────────────────────────────────────────────────
set(CMAKE_LINKER_TYPE LLD CACHE STRING "" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_INIT "-fuse-ld=lld")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-fuse-ld=lld")

# ── resource compiler ─────────────────────────────────────────────────────────
set(CMAKE_RC_COMPILER llvm-rc CACHE FILEPATH "" FORCE)

# ── find_* scoping ───────────────────────────────────────────────────────────
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)
