cpmaddpackage(
    NAME
    imgui
    VERSION
    1.92.6
    GITHUB_REPOSITORY
    ocornut/imgui
    DOWNLOAD_ONLY
    TRUE)

add_library(imgui STATIC)

target_sources(
    imgui
    PRIVATE ${imgui_SOURCE_DIR}/imgui.cpp
            ${imgui_SOURCE_DIR}/imgui_demo.cpp
            ${imgui_SOURCE_DIR}/imgui_draw.cpp
            ${imgui_SOURCE_DIR}/imgui_tables.cpp
            ${imgui_SOURCE_DIR}/imgui_widgets.cpp
            ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
            ${imgui_SOURCE_DIR}/misc/freetype/imgui_freetype.cpp)

target_include_directories(
    imgui SYSTEM PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/misc/cpp
                        ${imgui_SOURCE_DIR}/misc/freetype)

target_compile_definitions(imgui PUBLIC IMGUI_ENABLE_FREETYPE)
target_link_libraries(imgui PRIVATE freetype)

# OpenGL backend for Win32 (used by bass_proxy overlay)
add_library(imgui_opengl3 STATIC)

target_sources(
    imgui_opengl3 PRIVATE ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
                          ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)

target_include_directories(imgui_opengl3 SYSTEM
                           PUBLIC ${imgui_SOURCE_DIR}/backends)

target_link_libraries(imgui_opengl3 PUBLIC imgui glad opengl32)
