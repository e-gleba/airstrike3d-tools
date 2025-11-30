# Common dependencies for both engine and game
set(COMMON_LIBS
    glm
    EnTT::EnTT
    spdlog
)

# ============================================================================
# Engine executable
# ============================================================================
add_executable(engine)

file(GLOB_RECURSE ENGINE_SRC CONFIGURE_DEPENDS 
    ${CMAKE_CURRENT_LIST_DIR}/engine/*.cpp
)
target_sources(engine PRIVATE ${ENGINE_SRC})

target_include_directories(engine PRIVATE 
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/engine
    ${CMAKE_CURRENT_LIST_DIR}/shared
)

set_target_properties(engine PROPERTIES 
    CXX_STANDARD 26 
    CXX_STANDARD_REQUIRED ON 
    CXX_EXTENSIONS OFF
)

target_link_libraries(engine PRIVATE
    warnings
    ${COMMON_LIBS}
    SDL3::SDL3-static
    SDL3_mixer::SDL3_mixer-static
    SDL3_shadercross::SDL3_shadercross
    yaml-cpp
    stb
    imgui_sdl3
    tinyobjloader
)

# ============================================================================
# Game shared library
# ============================================================================
add_library(game SHARED)

file(GLOB_RECURSE GAME_SRC CONFIGURE_DEPENDS 
    ${CMAKE_CURRENT_LIST_DIR}/game/*.cpp
)
target_sources(game PRIVATE ${GAME_SRC})

target_include_directories(game PRIVATE 
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/engine
    ${CMAKE_CURRENT_LIST_DIR}/shared
)

set_target_properties(game PROPERTIES 
    CXX_STANDARD 26 
    CXX_STANDARD_REQUIRED ON 
    CXX_EXTENSIONS OFF
    # Output to same directory as engine
    LIBRARY_OUTPUT_DIRECTORY $<TARGET_FILE_DIR:engine>
    RUNTIME_OUTPUT_DIRECTORY $<TARGET_FILE_DIR:engine>
)

target_link_libraries(game PRIVATE
    warnings
    ${COMMON_LIBS}
    imgui  # Just imgui, not SDL backend - engine handles that
)

# ============================================================================
# Symlinks for shaders
# ============================================================================
set(SHADERS_SOURCE_DIR "${CMAKE_SOURCE_DIR}/shaders")

add_custom_command(
    TARGET engine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink
            "${SHADERS_SOURCE_DIR}"
            "$<TARGET_FILE_DIR:engine>/shaders"
    COMMENT "Creating symlink to shaders directory"
    VERBATIM
)
