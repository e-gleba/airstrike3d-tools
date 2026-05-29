cmake_minimum_required(VERSION 3.31)

project(
    as3d_reverse_toolkit
    VERSION 1.1.0
    DESCRIPTION "Airstrike divo games reverse engineering toolkit"
    HOMEPAGE_URL "https://github.com/e-gleba/airstrike3d-tools"
    LANGUAGES C CXX)

include(CMakePrintHelpers)

option(USE_BASS_PROXY_LIB "Build the BASS proxy DLL" ON)

# CMAKE_CURRENT_LIST_DIR is used instead of CMAKE_SOURCE_DIR so that this
# file works correctly if the project is ever consumed as a subdirectory of a
# larger build.  Ref: Professional CMake §8.3 "Project-relative Variables"
list(
    APPEND
    CMAKE_PREFIX_PATH
    ${CMAKE_CURRENT_LIST_DIR}/cmake
    ${CMAKE_CURRENT_LIST_DIR}/cmake/code_quality)

# When cross-compiling, CMAKE_FIND_ROOT_PATH_MODE_PACKAGE is typically set to
# ONLY in the toolchain file. That restricts find_package() to searching only
# under CMAKE_FIND_ROOT_PATH and CMAKE_SYSROOT — our local cmake/ dirs
# wouldn't be found. Adding them to CMAKE_FIND_ROOT_PATH fixes this.
if(CMAKE_CROSSCOMPILING)
    list(
        APPEND
        CMAKE_FIND_ROOT_PATH
        ${CMAKE_CURRENT_LIST_DIR}/cmake
        ${CMAKE_CURRENT_LIST_DIR}/cmake/code_quality)
endif()

cmake_print_variables(
    CMAKE_PREFIX_PATH
    CMAKE_FIND_ROOT_PATH
    CMAKE_CROSSCOMPILING
    CMAKE_SYSTEM_NAME
    CMAKE_CXX_COMPILER_ID
    CMAKE_CXX_COMPILER_VERSION)

find_package(warnings CONFIG REQUIRED)

if(PROJECT_IS_TOP_LEVEL)
    find_package(clang_format CONFIG REQUIRED)
    find_package(clang_tidy CONFIG REQUIRED)
    find_package(clang_doc CONFIG REQUIRED)
endif()

add_subdirectory(external SYSTEM)
find_package(cpm CONFIG REQUIRED)

# ─── SDK (version-agnostic, compiled once) ────────────────────────────────────
add_subdirectory(src)

# ─── Game version deployment ──────────────────────────────────────────────────
# Each 2_XX subdirectory deploys one game version and creates its own
# bass_proxy_<version> target (linking bass_proxy_sdk) via add_bass_proxy().
# Outputs land in build/<version>/ — no collisions.

set(GAME_VERSION "2_06")
set(GAME_EXE_NAME "as3d2.exe")
add_subdirectory(2_06)

set(GAME_VERSION "2_51")
set(GAME_EXE_NAME "as3d2.exe")
add_subdirectory(2_51)

set(GAME_VERSION "2_71")
set(GAME_EXE_NAME "AirStrike3D II - Gulf.exe")
add_subdirectory(2_71)

# CTest and CPack rules should only run when this project owns the build.
# If another project pulls us in via add_subdirectory() or FetchContent,
# it controls testing and packaging — we should not interfere.
# Ref: Professional CMake §8.3, §39 "FetchContent"
if(PROJECT_IS_TOP_LEVEL)
    include(CPack)
endif()
