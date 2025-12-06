file(GLOB_RECURSE src CONFIGURE_DEPENDS ${CMAKE_CURRENT_LIST_DIR}/*.cpp)

add_library(model-loaders SHARED ${src})

target_include_directories(model-loaders PUBLIC ${CMAKE_CURRENT_LIST_DIR})

set_target_properties(
    model-loaders
    PROPERTIES CXX_STANDARD 26 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF
)

target_link_libraries(
    model-loaders
    PRIVATE warnings glm spdlog tinyobjloader tinygltf
)
