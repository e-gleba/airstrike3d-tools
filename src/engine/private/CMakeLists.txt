add_executable(engine)

file(
    GLOB_RECURSE engine_src
    CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)
target_sources(engine PRIVATE ${engine_src})

target_include_directories(engine PRIVATE ${CMAKE_CURRENT_LIST_DIR})

set_target_properties(
    engine
    PROPERTIES CXX_STANDARD 26 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF
)

add_subdirectory(model-loaders)

target_link_libraries(
    engine
    PRIVATE
        warnings
        glm
        EnTT::EnTT
        spdlog
        SDL3::SDL3-static
        SDL3_mixer::SDL3_mixer-static
        SDL3_shadercross::SDL3_shadercross
        yaml-cpp
        stb
        imgui_sdl3
        tinyobjloader
        tinygltf
        pybind11::embed
        model-loaders
        engine_interface
)

add_custom_command(
    TARGET engine
    POST_BUILD
    COMMAND
        ${CMAKE_COMMAND} -E create_symlink "${CMAKE_SOURCE_DIR}/shaders"
        "$<TARGET_FILE_DIR:engine>/shaders"
    COMMAND
        ${CMAKE_COMMAND} -E create_symlink "${CMAKE_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:engine>/assets"
    COMMENT "Creating symlinks to shaders and assets"
    VERBATIM
)
