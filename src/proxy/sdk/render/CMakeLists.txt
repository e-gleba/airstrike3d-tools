# Render abstraction - static library
# Simple flat structure: all sources in module root

add_library(sdk_render STATIC
    renderer.cpp
    opengl_renderer.cpp
    opengl_hooks.cpp
    directx8_renderer.cpp
)

target_include_directories(sdk_render PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_compile_features(sdk_render PUBLIC cxx_std_23)

# Link dependencies
target_link_libraries(sdk_render PUBLIC
    sdk_math
    sdk_platform
)

# OpenGL
find_package(OpenGL QUIET)
if(OpenGL_FOUND)
    target_link_libraries(sdk_render PRIVATE OpenGL::GL)
endif()

set_target_properties(sdk_render PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    FOLDER "SDK"
)
