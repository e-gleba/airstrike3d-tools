# Math Library - Static library with GLM isolated in implementation
# Simple flat structure: all sources in module root

add_library(sdk_math STATIC
    glm_adapter.cpp
)

target_include_directories(sdk_math PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_compile_features(sdk_math PUBLIC cxx_std_23)

# GLM for internal implementation only
find_package(glm REQUIRED)
target_link_libraries(sdk_math PRIVATE glm::glm)

set_target_properties(sdk_math PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    FOLDER "SDK"
)
