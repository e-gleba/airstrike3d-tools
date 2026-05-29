# src/proxy/CMakeLists.txt
#
# Architecture:
#   bass_proxy_sdk (OBJECT) — compiled once, shared across all versions.
#     Holds the SDK source: hooks, overlay, Lua engine, utilities.
#
#   bass_proxy_<version> (SHARED) — one per game version.
#     Each links bass_proxy_sdk and uses a .def generated from that
#     version's bass.dll, so the proxy DLL matches the exact export
#     table of the bass.dll it replaces.
#
# Dependencies (via CPM): warnings, imgui_opengl3, dwmapi, glm, glu32,
# opengl32, safetyhook, spdlog, sol2, lua.

include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/proxy_utils.cmake")

# ─── SDK object library (compiled once) ───────────────────────────────────────

file(
    GLOB_RECURSE
    sdk_sources
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/*.cpp")

add_library(bass_proxy_sdk OBJECT ${sdk_sources})

target_include_directories(bass_proxy_sdk PUBLIC "${CMAKE_CURRENT_LIST_DIR}")

# target_compile_features does not support generator expressions.
# Use compiler-specific flags instead: /std:c++latest for MSVC,
# -std=c++26 for Clang/GCC.
target_compile_options(
    bass_proxy_sdk
    PUBLIC
    $<$<CXX_COMPILER_ID:MSVC>:/std:c++latest>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-std=c++26>)

set_target_properties(bass_proxy_sdk PROPERTIES CXX_EXTENSIONS OFF)

target_compile_definitions(
    bass_proxy_sdk
    PUBLIC WIN32_LEAN_AND_MEAN
           NOMINMAX
           GLM_FORCE_RADIANS
           GLM_FORCE_DEPTH_ZERO_TO_ONE)

# ─── Per-version proxy targets ────────────────────────────────────────────────

set(proxy_versions "2_06" "2_51" "2_71")

foreach(ver IN LISTS proxy_versions)
    set(bass_dll "${CMAKE_SOURCE_DIR}/${ver}/bass.dll")

    if(NOT EXISTS "${bass_dll}")
        message(
            WARNING
                "bass.dll not found at '${bass_dll}' — skipping proxy for ${ver}"
        )
        continue()
    endif()

    add_versioned_proxy(VERSION "${ver}" BASS_DLL "${bass_dll}")
endforeach()
