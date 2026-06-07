# glad — OpenGL loader (third-party, warnings suppressed)
# https://glad.dav1d.de/
add_library(glad STATIC "${CMAKE_CURRENT_LIST_DIR}/glad/src/glad.c")

target_include_directories(
    glad SYSTEM
    PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/glad/include>")

target_compile_features(glad PRIVATE c_std_11)

set_target_properties(glad PROPERTIES POSITION_INDEPENDENT_CODE ON)
