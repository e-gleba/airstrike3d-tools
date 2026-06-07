# Math Library
# Pure C++23 math types and operations (GLM isolated in implementation)

add_library(sdk_math STATIC)

# Source files (implementation - GLM isolated here)
target_sources(sdk_math PRIVATE
    src/glm_adapter.cpp
)

# Public headers
target_sources(sdk_math PUBLIC
    FILE_SET HEADERS
    BASE_DIRS include
    FILES
        include/sdk/math/types.hpp
        include/sdk/math/vec3.hpp
        include/sdk/math/mat4.hpp
        include/sdk/math/operations.hpp
)

# Include directories
target_include_directories(sdk_math
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# C++23 standard
target_compile_features(sdk_math PUBLIC cxx_std_23)

# Link GLM (implementation detail, not exposed in public headers)
find_package(glm REQUIRED)
target_link_libraries(sdk_math PRIVATE glm::glm)

# Enable warnings
target_compile_options(sdk_math PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra -Wpedantic>
)

# Set library properties
set_target_properties(sdk_math PROPERTIES
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
    EXPORT_NAME math
)

# Install rules
install(TARGETS sdk_math
    EXPORT sdk_targets
    FILE_SET HEADERS
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)
