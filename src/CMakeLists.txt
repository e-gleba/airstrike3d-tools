add_executable(game main.cpp)

target_include_directories(game PRIVATE ${CMAKE_CURRENT_LIST_DIR})

set_target_properties(
    game
    PROPERTIES
    CXX_STANDARD 26
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

target_link_libraries(game PRIVATE
    warnings
    SDL3::SDL3
    SDL3_mixer::SDL3_mixer
    yaml-cpp
    raylib
)