# src/proxy/CMakeLists.txt
#
# bass_proxy_sdk — STATIC library, compiled once.
# Contains all SDK code: hooks, overlay, Lua engine, utilities.
# Version-agnostic.  Each 2_XX/ directory creates its own proxy
# SHARED target by linking against this library via add_bass_proxy().
#
# Dependencies (via CPM): warnings, imgui_opengl3, dwmapi, glm, glu32,
# opengl32, safetyhook, spdlog, lua, LuaBridge3.

set(sdk_sources
    dll_main.cpp
    sdk/core/hooks.cpp
    sdk/core/detail/logging_spdlog.cpp
    sdk/graphics/rendering.cpp
    sdk/graphics/detail/camera_math.cpp
    sdk/graphics/detail/d3d8_hooks.cpp
    sdk/graphics/detail/gl_hooks_opengl.cpp
    sdk/graphics/detail/graphics_opengl.cpp
    sdk/math/detail/math_glm.cpp
    sdk/overlay/detail/overlay_imgui.cpp
    sdk/platform/detail/platform_win32.cpp
    sdk/scripting/detail/engine_lua.cpp
    sdk/ui/detail/ui_imgui.cpp)

add_library(
    imgui_d3d8_as3d STATIC
    sdk/overlay/detail/imgui_impl_d3d8_as3d.cpp)
target_include_directories(imgui_d3d8_as3d PRIVATE "${CMAKE_CURRENT_LIST_DIR}")
target_compile_features(imgui_d3d8_as3d PUBLIC cxx_std_23)
target_compile_definitions(imgui_d3d8_as3d PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(imgui_d3d8_as3d PUBLIC imgui::imgui d3d8)

add_library(bass_proxy_sdk STATIC ${sdk_sources})

target_include_directories(bass_proxy_sdk PRIVATE "${CMAKE_CURRENT_LIST_DIR}")

target_compile_features(bass_proxy_sdk PUBLIC cxx_std_23)

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
            imgui_win32
            imgui_opengl3
            imgui_d3d8_as3d
            d3d8
            dwmapi
            glm
            glu32
            opengl32
            safetyhook
            spdlog::spdlog
            lua
            LuaBridge)
