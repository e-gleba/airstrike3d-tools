cpmaddpackage(
    NAME
    imgui
    VERSION
    1.92.7
    GITHUB_REPOSITORY
    ocornut/imgui
    EXCLUDE_FROM_ALL
    ON
    DOWNLOAD_ONLY
    TRUE)

add_library(imgui STATIC)
add_library(imgui::imgui ALIAS imgui)

target_sources(
    imgui
    PRIVATE ${imgui_SOURCE_DIR}/imgui.cpp
            ${imgui_SOURCE_DIR}/imgui_demo.cpp
            ${imgui_SOURCE_DIR}/imgui_draw.cpp
            ${imgui_SOURCE_DIR}/imgui_tables.cpp
            ${imgui_SOURCE_DIR}/imgui_widgets.cpp
            ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp)

target_include_directories(
    imgui SYSTEM PUBLIC $<BUILD_INTERFACE:${imgui_SOURCE_DIR}>
                        $<BUILD_INTERFACE:${imgui_SOURCE_DIR}/misc/cpp>)

target_compile_features(imgui PUBLIC cxx_std_23)

# FreeType is fetched via CPM (add_subdirectory), not find_package, so we check
# for the target name instead of Freetype_FOUND / Freetype::Freetype.
if(TARGET freetype)
    target_sources(
        imgui PRIVATE ${imgui_SOURCE_DIR}/misc/freetype/imgui_freetype.cpp)

    target_link_libraries(imgui PUBLIC freetype)
    target_include_directories(
        imgui SYSTEM
        PUBLIC $<BUILD_INTERFACE:${imgui_SOURCE_DIR}/misc/freetype>)
    target_compile_definitions(imgui PUBLIC IMGUI_ENABLE_FREETYPE)
else()
    message(
        STATUS
        "imgui: freetype target not available — custom font rasterizer disabled."
    )
endif()

# Windows + OpenGL backend for the BASS proxy overlay.
add_library(imgui_opengl3 STATIC)

target_sources(
    imgui_opengl3 PRIVATE ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
                          ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)

target_include_directories(
    imgui_opengl3 SYSTEM
    PUBLIC $<BUILD_INTERFACE:${imgui_SOURCE_DIR}/backends>)

target_link_libraries(imgui_opengl3 PUBLIC imgui::imgui glad opengl32)
