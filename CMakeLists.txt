cmake_minimum_required(VERSION 3.31)

project(
    as3d_reverse_toolkit
    VERSION 1.0.9
    DESCRIPTION "Airstrike divo games reverse engineering toolkit"
    HOMEPAGE_URL "https://github.com/e-gleba/airstrike3d-tools"
    LANGUAGES C CXX)

# Keep project-owned paths stable when this repository is consumed through
# add_subdirectory(). CMAKE_SOURCE_DIR refers to the embedding project there.
set(AS3D_PROJECT_ROOT "${CMAKE_CURRENT_LIST_DIR}")

option(USE_BASS_PROXY_LIB "Build the BASS proxy DLL" ON)
option(AS3D_ENABLE_TESTS "Build and register CTest emulator tests" ON)
set(AS3D_EMULATOR_TEST_TIMEOUT
    "5"
    CACHE STRING "Timeout (seconds) for emulator smoke-launch tests")

list(
    APPEND
    CMAKE_PREFIX_PATH
    "${AS3D_PROJECT_ROOT}/cmake"
    "${AS3D_PROJECT_ROOT}/cmake/code_quality")

if(CMAKE_CROSSCOMPILING)
    list(
        APPEND
        CMAKE_FIND_ROOT_PATH
        "${AS3D_PROJECT_ROOT}/cmake"
        "${AS3D_PROJECT_ROOT}/cmake/code_quality")
endif()

add_library(as3d_build_options INTERFACE)
target_compile_features(as3d_build_options INTERFACE cxx_std_23)
set_target_properties(as3d_build_options PROPERTIES INTERFACE_CXX_EXTENSIONS OFF)

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
    add_subdirectory(tests/proxy)
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
