# cmake/toolchains/llvm_mingw.cmake
#
# Cross-compilation toolchain: Linux host -> Windows target (llvm-mingw).
# Usage: cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/llvm_mingw.cmake \
#              -DCMAKE_SYSTEM_PROCESSOR=i686
#
# Tunables (all overridable via -D or cache):
#   LLVM_MINGW_VERSION       — release tag  (default: 20260421)
#   LLVM_MINGW_HOST_OS       — package OS suffix (default: ubuntu-22.04)
#   LLVM_MINGW_AUTO_DOWNLOAD — fetch if absent   (default: ON)

include_guard(GLOBAL)

# ── target system ─────────────────────────────────────────────────────────────
# Must be set before project() sees the toolchain file.
set(CMAKE_SYSTEM_NAME Windows)

if(NOT DEFINED CMAKE_SYSTEM_PROCESSOR)
    set(CMAKE_SYSTEM_PROCESSOR i686)
endif()

# ── tunables ──────────────────────────────────────────────────────────────────
set(LLVM_MINGW_VERSION
    "20260421"
    CACHE STRING "llvm-mingw release tag")
set(LLVM_MINGW_HOST_OS
    "ubuntu-22.04"
    CACHE STRING "llvm-mingw host OS package suffix")
option(LLVM_MINGW_AUTO_DOWNLOAD "Download llvm-mingw if absent" ON)

# ── host arch ─────────────────────────────────────────────────────────────────
if(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
    set(llvm_mingw_host_arch aarch64)
else()
    set(llvm_mingw_host_arch x86_64)
endif()

# ── target triple ─────────────────────────────────────────────────────────────
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
    set(llvm_mingw_triple aarch64-w64-mingw32)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$")
    set(llvm_mingw_triple x86_64-w64-mingw32)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^i[3-6]86$")
    set(llvm_mingw_triple i686-w64-mingw32)
else()
    message(
        FATAL_ERROR
            "llvm_mingw: unsupported CMAKE_SYSTEM_PROCESSOR='${CMAKE_SYSTEM_PROCESSOR}'\n"
            "Supported: x86_64, i686, aarch64")
endif()

# ── paths ─────────────────────────────────────────────────────────────────────
cmake_path(
    GET
    CMAKE_CURRENT_LIST_DIR
    PARENT_PATH
    llvm_mingw_cmake_dir)
cmake_path(
    GET
    llvm_mingw_cmake_dir
    PARENT_PATH
    llvm_mingw_project_root)

set(llvm_mingw_install_dir "${llvm_mingw_project_root}/llvm_mingw")
set(llvm_mingw_pkg
    "llvm-mingw-${LLVM_MINGW_VERSION}-ucrt-${LLVM_MINGW_HOST_OS}-${llvm_mingw_host_arch}"
)
set(llvm_mingw_url
    "https://github.com/mstorsjo/llvm-mingw/releases/download/${LLVM_MINGW_VERSION}/${llvm_mingw_pkg}.tar.xz"
)
set(llvm_mingw_archive "${llvm_mingw_project_root}/${llvm_mingw_pkg}.tar.xz")
set(llvm_mingw_sysroot "${llvm_mingw_install_dir}/${llvm_mingw_triple}")

# ── download / extract ────────────────────────────────────────────────────────
if(NOT EXISTS "${llvm_mingw_install_dir}/bin/${llvm_mingw_triple}-clang")
    if(NOT LLVM_MINGW_AUTO_DOWNLOAD)
        message(
            FATAL_ERROR
                "llvm-mingw: toolchain not found at '${llvm_mingw_install_dir}'\n"
                "Set -DLLVM_MINGW_AUTO_DOWNLOAD=ON or extract '${llvm_mingw_pkg}' there manually.")
    endif()

    message(STATUS "llvm-mingw: fetching '${llvm_mingw_pkg}'")
    file(
        DOWNLOAD "${llvm_mingw_url}" "${llvm_mingw_archive}"
        SHOW_PROGRESS
        STATUS llvm_mingw_dl_status
        TLS_VERIFY ON)

    list(
        GET
        llvm_mingw_dl_status
        0
        llvm_mingw_dl_code)
    if(NOT
       llvm_mingw_dl_code
       EQUAL
       0)
        list(
            GET
            llvm_mingw_dl_status
            1
            llvm_mingw_dl_msg)
        file(REMOVE "${llvm_mingw_archive}")
        message(
            FATAL_ERROR "llvm-mingw: download failed => ${llvm_mingw_dl_msg}")
    endif()

    message(STATUS "llvm-mingw: extracting '${llvm_mingw_pkg}.tar.xz'")
    file(
        ARCHIVE_EXTRACT
        INPUT
        "${llvm_mingw_archive}"
        DESTINATION
        "${llvm_mingw_project_root}")
    file(REMOVE_RECURSE "${llvm_mingw_install_dir}")
    file(RENAME "${llvm_mingw_project_root}/${llvm_mingw_pkg}"
         "${llvm_mingw_install_dir}")
    file(REMOVE "${llvm_mingw_archive}")
    message(STATUS "llvm-mingw: installed => '${llvm_mingw_install_dir}'")
endif()

# ── compilers & tools ─────────────────────────────────────────────────────────
set(CMAKE_C_COMPILER
    "${llvm_mingw_install_dir}/bin/${llvm_mingw_triple}-clang"
    CACHE FILEPATH "" FORCE)
set(CMAKE_CXX_COMPILER
    "${llvm_mingw_install_dir}/bin/${llvm_mingw_triple}-clang++"
    CACHE FILEPATH "" FORCE)
set(CMAKE_RC_COMPILER
    "${llvm_mingw_install_dir}/bin/${llvm_mingw_triple}-windres"
    CACHE FILEPATH "" FORCE)
set(CMAKE_AR
    "${llvm_mingw_install_dir}/bin/llvm-ar"
    CACHE FILEPATH "" FORCE)
set(CMAKE_RANLIB
    "${llvm_mingw_install_dir}/bin/llvm-ranlib"
    CACHE FILEPATH "" FORCE)
set(CMAKE_LINKER
    "${llvm_mingw_install_dir}/bin/ld.lld"
    CACHE FILEPATH "" FORCE)

# ── sysroot ───────────────────────────────────────────────────────────────────
set(CMAKE_SYSROOT "${llvm_mingw_sysroot}")

set(CMAKE_C_FLAGS_INIT "--sysroot=${llvm_mingw_sysroot}")
set(CMAKE_CXX_FLAGS_INIT "--sysroot=${llvm_mingw_sysroot}")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-fuse-ld=lld --sysroot=${llvm_mingw_sysroot}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT
    "-fuse-ld=lld --sysroot=${llvm_mingw_sysroot}")

# ── find_* scoping ────────────────────────────────────────────────────────────
set(CMAKE_FIND_ROOT_PATH "${llvm_mingw_sysroot}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# ── clang-tidy ────────────────────────────────────────────────────────────────
# llvm-mingw releases do not ship clang-tidy. The host's clang-tidy is
# version-mismatched against the bundled libc++ headers and produces false
# errors. Disable unconditionally for this toolchain.
set(CMAKE_C_CLANG_TIDY
    ""
    CACHE STRING "" FORCE)
set(CMAKE_CXX_CLANG_TIDY
    ""
    CACHE STRING "" FORCE)
