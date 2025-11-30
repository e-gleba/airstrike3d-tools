add_executable(game main.cpp)

file(GLOB_RECURSE src CONFIGURE_DEPENDS ${CMAKE_CURRENT_LIST_DIR}/*.cpp)
target_sources(game PRIVATE ${src})
target_include_directories(game PRIVATE ${CMAKE_CURRENT_LIST_DIR})

set_target_properties(
    game
    PROPERTIES CXX_STANDARD 26 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF
)

target_link_libraries(
    game
    PRIVATE
        warnings
        SDL3::SDL3-static
        SDL3_mixer::SDL3_mixer-static
        SDL3_shadercross::SDL3_shadercross
        yaml-cpp
        stb
        glm
        imgui_sdl3
        EnTT::EnTT
        spdlog
        tinyobjloader
        sol2::sol2
        BulletDynamics # Rigid body dynamics
        BulletCollision # Collision detection
        LinearMath # Math library
)
