add_library(game SHARED)

file(GLOB_RECURSE GAME_SRC CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
target_sources(game PRIVATE ${GAME_SRC})

target_include_directories(game PRIVATE ${CMAKE_CURRENT_LIST_DIR})

set_target_properties(
    game
    PROPERTIES
        CXX_STANDARD 26
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        LIBRARY_OUTPUT_DIRECTORY $<TARGET_FILE_DIR:engine>
        RUNTIME_OUTPUT_DIRECTORY $<TARGET_FILE_DIR:engine>
)

target_link_libraries(
    game
    PRIVATE warnings glm EnTT::EnTT spdlog implot imgui engine_interface tinygltf
)