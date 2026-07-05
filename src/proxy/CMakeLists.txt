# src/proxy/CMakeLists.txt
#
# bass_proxy_sdk — STATIC library, compiled once.
# Contains all SDK code: hooks, overlay, Lua engine, utilities.
# Version-agnostic.  Each 2_XX/ directory creates its own proxy
# SHARED target by linking against this library via add_bass_proxy().
#
# Dependencies (via CPM): warnings, imgui_opengl3, dwmapi, glm, glu32,
# opengl32, safetyhook, spdlog, sol2, lua, LuaBridge3 (optional).
#
# Lua backend selection:
#   SDK_EXPERIMENTAL_LUABRIDGE3=OFF (default) → engine_lua.cpp (sol2)
#   SDK_EXPERIMENTAL_LUABRIDGE3=ON           → engine_luabridge3.cpp (LuaBridge3)
# Only one backend is compiled; the other is excluded from sources.

file(
    GLOB_RECURSE
    sdk_sources
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/*.cpp")

# ─── Lua backend selection ────────────────────────────────────────────────────
# GLOB_RECURSE picks up both engine_lua.cpp and engine_luabridge3.cpp.
# Exclude the unused backend to prevent duplicate symbol errors.
if(SDK_EXPERIMENTAL_LUABRIDGE3)
    list(REMOVE_ITEM sdk_sources
         "${CMAKE_CURRENT_LIST_DIR}/sdk/scripting/detail/engine_lua.cpp")
    message(STATUS "bass_proxy_sdk: using experimental LuaBridge3 backend")
else()
    list(REMOVE_ITEM sdk_sources
         "${CMAKE_CURRENT_LIST_DIR}/sdk/scripting/detail/engine_luabridge3.cpp")
    message(STATUS "bass_proxy_sdk: using sol2 backend (default)")
endif()

add_library(bass_proxy_sdk STATIC ${sdk_sources})

target_include_directories(bass_proxy_sdk PRIVATE "${CMAKE_CURRENT_LIST_DIR}")

# target_compile_features does not support generator expressions.
# Use compiler-specific flags instead: /std:c++latest for MSVC,
# -std=c++26 for Clang/GCC.
target_compile_options(
    bass_proxy_sdk PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/std:c++latest>
                           $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-std=c++26>)

set_target_properties(
    bass_proxy_sdk
    PROPERTIES CXX_EXTENSIONS OFF
               CXX_SCAN_FOR_MODULES OFF
               UNITY_BUILD ON
               UNITY_BUILD_MODE BATCH # or GROUP if you want manual control
               UNITY_BUILD_BATCH_SIZE 16 # tune: 8–32 is usually a sweet spot
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
            # ─── Lua binding library (backend-dependent) ─────────────────────
            # sol2 always linked (header-only, no cost when not used).
            # LuaBridge linked only when experimental backend selected.
            sol2::sol2
            lua
            $<$<BOOL:${SDK_EXPERIMENTAL_LUABRIDGE3}>:LuaBridge>)
