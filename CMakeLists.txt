cmake_minimum_required(VERSION 3.31)

project(
    as3d_reverse_toolkit
    VERSION 1.0.9
    DESCRIPTION "Airstrike divo games reverse engineering toolkit"
    HOMEPAGE_URL "https://github.com/e-gleba/airstrike3d-tools"
    LANGUAGES C CXX)

include(CMakePrintHelpers)

set(CMAKE_UNITY_BUILD ON)

option(USE_BASS_PROXY_LIB "Build the BASS proxy DLL" ON)
option(AS3D_ENABLE_TESTS "Build and register CTest emulator tests" ON)
set(AS3D_EMULATOR_TEST_TIMEOUT
    "5"
    CACHE STRING "Timeout (seconds) for emulator smoke-launch tests")

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

# ─── CTest (must be enabled BEFORE add_subdirectory for version dirs) ─────────
# include(CTest) calls enable_testing() in the current scope. Test
# registration happens inside subdirectories (2_06, 2_51, 2_71) via
# deploy_game.cmake, CTest must already be active or add_test() silently
# does nothing and CTest reports "No tests were found!!!".
if(PROJECT_IS_TOP_LEVEL AND AS3D_ENABLE_TESTS)
    include(CTest)
endif()

add_subdirectory(external SYSTEM)
find_package(cpm CONFIG REQUIRED)

# ─── SDK (version-agnostic, compiled once) ────────────────────────────────────
add_subdirectory(src)

# ─── Game version deploy ──────────────────────────────────────────────────────
# Each 2_XX subdirectory deploys one game version and creates its own
# bass_proxy_<version> target (linking bass_proxy_sdk) via add_bass_proxy().
# Outputs land in build/<version>/ — no collisions.

add_subdirectory(2_06)

add_subdirectory(2_51)

add_subdirectory(2_71)

# CTest and CPack rules should only run when this project owns the build.
# If another project pulls us in via add_subdirectory() or FetchContent,
# it controls testing and packaging — we should not interfere.
# Ref: Professional CMake §8.3, §39 "FetchContent"
if(PROJECT_IS_TOP_LEVEL)
    include(CPack)
endif()
