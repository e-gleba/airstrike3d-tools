# src/proxy/CMakeLists.txt
#
# bass_proxy_sdk — STATIC library with ultimate C++23 architecture refactoring.
#
# Architecture:
#   - All external dependencies (sol2, WinAPI, GLM, OpenGL, safetyhook, ImGui)
#     are hidden behind pure C++23 abstraction layers.
#   - Public headers contain NO external dependencies.
#   - Implementation files (.cpp) isolate all third-party code.
#   - RAII pattern used throughout (no manual init/shutdown).
#   - C++23 concepts, constexpr, std::expected applied extensively.
#
# Style references:
#   - Antony Polukhin: PIMPL, ABI-stable interfaces
#   - Jason Turner: Modern C++23, constexpr, concepts
#   - Stefano Tessarin: Concept-constrained operations

file(
    GLOB_RECURSE
    sdk_sources
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/*.cpp")

add_library(bass_proxy_sdk STATIC ${sdk_sources})

target_include_directories(bass_proxy_sdk PRIVATE "${CMAKE_CURRENT_LIST_DIR}")

# C++23 standard (C++26 compile flag for latest features)
target_compile_options(
    bass_proxy_sdk PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/std:c++latest>
                           $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-std=c++26>)

set_target_properties(
    bass_proxy_sdk
    PROPERTIES CXX_EXTENSIONS OFF
               CXX_SCAN_FOR_MODULES OFF
               UNITY_BUILD ON
               UNITY_BUILD_MODE BATCH
               UNITY_BUILD_BATCH_SIZE 16
    )

target_compile_definitions(
    bass_proxy_sdk
    PRIVATE WIN32_LEAN_AND_MEAN
            NOMINMAX
            GLM_FORCE_RADIANS
            GLM_FORCE_DEPTH_ZERO_TO_ONE)

target_link_libraries(
    bass_proxy_sdk
    PRIVATE warnings
            imgui_opengl3
            dwmapi
            glm
            glu32
            opengl32
            safetyhook
            spdlog::spdlog
            sol2::sol2
            lua
            $<$<BOOL:${SDK_EXPERIMENTAL_LUABRIDGE3}>:LuaBridge>)

# ─── Architecture documentation ──────────────────────────────────────────────
#
# New directory structure:
#   sdk/api/           — Pure C++23 types (math, render_api, callbacks, key_codes)
#   sdk/core/          — RAII engine class (replaces global g_ctx)
#   sdk/platform/      — WinAPI wrappers (window, module, input)
#   sdk/math/          — Math operations (GLM hidden in .cpp)
#   sdk/scripting/     — Lua abstraction (sol2 hidden in .cpp)
#   sdk/hooking/       — Hook abstraction (safetyhook hidden in .cpp)
#   sdk/render/        — OpenGL abstraction (GL/gl.h hidden in .cpp)
#   sdk/overlay/       — ImGui abstraction (ImGui hidden in .cpp)
#
# All public headers (.hpp) contain NO external dependencies.
# All third-party code isolated in implementation files (.cpp).
