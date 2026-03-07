# https://glad.dav1d.de/
add_library(glad STATIC "${CMAKE_CURRENT_LIST_DIR}/glad/src/glad.c")

target_include_directories(
    glad SYSTEM
    PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/glad/include>")

set_target_properties(
    glad
    PROPERTIES C_STANDARD 11
               C_STANDARD_REQUIRED ON
               C_EXTENSIONS OFF
               POSITION_INDEPENDENT_CODE ON)

