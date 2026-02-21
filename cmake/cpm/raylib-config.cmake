cpmaddpackage(
    NAME
    raylib
    GITHUB_REPOSITORY
    raysan5/raylib
    GIT_TAG
    5.5
    GIT_SHALLOW
    TRUE
    OPTIONS
    "BUILD_EXAMPLES OFF"
    "BUILD_GAMES OFF"
    "OPENGL_VERSION 2.1")

cpmaddpackage(
    NAME
    rlimgui
    GITHUB_REPOSITORY
    raylib-extras/rlImGui
    GIT_TAG
    main
    GIT_SHALLOW
    TRUE
    DOWNLOAD_ONLY
    YES)

if(rlimgui_ADDED)
    add_library(rlimgui STATIC ${rlimgui_SOURCE_DIR}/rlImGui.cpp)
    target_include_directories(rlimgui SYSTEM PUBLIC ${rlimgui_SOURCE_DIR})
    target_link_libraries(rlimgui PRIVATE raylib imgui)
endif()
