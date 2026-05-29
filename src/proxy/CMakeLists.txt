# src/proxy/CMakeLists.txt
#
# bass_proxy_sdk — STATIC library, compiled once.
# Contains all SDK code: hooks, overlay, Lua engine, utilities.
# Version-agnostic.  Each 2_XX/ directory creates its own proxy
# SHARED target by linking against this library via add_bass_proxy().
#
# Dependencies (via CPM): warnings, imgui_opengl3, imgui_dx8, dwmapi,
# glm, glu32, opengl32, safetyhook, spdlog, sol2, lua.
#
# NOTE: d3d8 is NOT linked — <d3d8.h> provides only type definitions.
# All DX8 hooking uses dynamic resolution (GetProcAddress) to avoid
# creating a static import dependency that would fail the DLL load
# if d3d8.dll isn't present at load time (Wine/Proton load ordering).

file(
    GLOB_RECURSE
    sdk_sources
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/*.cpp")

add_library(bass_proxy_sdk STATIC ${sdk_sources})

target_include_directories(bass_proxy_sdk PUBLIC "${CMAKE_CURRENT_LIST_DIR}")

# target_compile_features does not support generator expressions.
# Use compiler-specific flags instead: /std:c++latest for MSVC,
# -std=c++26 for Clang/GCC.
target_compile_options(
    bass_proxy_sdk PUBLIC $<$<CXX_COMPILER_ID:MSVC>:/std:c++latest>
                          $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-std=c++26>)

set_target_properties(
    bass_proxy_sdk
    PROPERTIES CXX_EXTENSIONS OFF
               UNITY_BUILD ON
               UNITY_BUILD_MODE BATCH # or GROUP if you want manual control
               UNITY_BUILD_BATCH_SIZE 16 # tune: 8–32 is usually a sweet spot
    )

target_compile_definitions(
    bass_proxy_sdk
    PUBLIC WIN32_LEAN_AND_MEAN
           NOMINMAX
           GLM_FORCE_RADIANS
           GLM_FORCE_DEPTH_ZERO_TO_ONE)

target_link_libraries(
    bass_proxy_sdk
    PUBLIC warnings
           imgui_opengl3
           imgui_dx8
           dwmapi
           glm
           glu32
           opengl32
           safetyhook
           spdlog::spdlog
           sol2::sol2
           lua)
