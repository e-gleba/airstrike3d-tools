# Platform Abstraction Library
# WinAPI wrappers with pure C++23 interfaces

add_library(sdk_platform STATIC)

# Source files
target_sources(sdk_platform PRIVATE
    src/window.cpp
    src/module.cpp
    src/input.cpp
)

# Public headers
target_sources(sdk_platform PUBLIC
    FILE_SET HEADERS
    BASE_DIRS include
    FILES
        include/sdk/platform/types.hpp
        include/sdk/platform/window.hpp
        include/sdk/platform/module.hpp
        include/sdk/platform/input.hpp
        include/sdk/platform/key_codes.hpp
)

# Include directories
target_include_directories(sdk_platform
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# C++23 standard
target_compile_features(sdk_platform PUBLIC cxx_std_23)

# Platform-specific definitions
if(WIN32)
    target_compile_definitions(sdk_platform PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        UNICODE
        _UNICODE
    )
endif()

# Enable warnings
target_compile_options(sdk_platform PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra -Wpedantic>
)

# Set library properties
set_target_properties(sdk_platform PROPERTIES
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
    EXPORT_NAME platform
)

# Install rules
install(TARGETS sdk_platform
    EXPORT sdk_targets
    FILE_SET HEADERS
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)
