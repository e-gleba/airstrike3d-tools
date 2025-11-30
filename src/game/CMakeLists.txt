add_library(game SHARED)

file(GLOB_RECURSE GAME_SRC CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
target_sources(game PRIVATE ${GAME_SRC})

target_include_directories(game 
    PUBLIC  ${SHARED_INCLUDE_DIR}
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

set_target_properties(game PROPERTIES 
    CXX_STANDARD 26 
    CXX_STANDARD_REQUIRED ON 
    CXX_EXTENSIONS OFF
    LIBRARY_OUTPUT_DIRECTORY $<TARGET_FILE_DIR:engine>
    RUNTIME_OUTPUT_DIRECTORY $<TARGET_FILE_DIR:engine>
)

# Find Lua using sol2's cmake module path
list(APPEND CMAKE_MODULE_PATH "${sol2_SOURCE_DIR}/cmake/Packages")
list(APPEND CMAKE_MODULE_PATH "${sol2_SOURCE_DIR}/cmake/Modules")
set(SOL2_LUA_VERSION "5.4.4")
set(SOL2_BUILD_LUA TRUE)
find_package(LuaBuild REQUIRED COMPONENTS 5.4.4)

target_link_libraries(game PRIVATE
    warnings
    glm
    EnTT::EnTT
    spdlog
    imgui
    yaml-cpp
    sol2
    ${LUA_LIBRARIES}
)

target_include_directories(game PRIVATE ${LUA_INCLUDE_DIR})

