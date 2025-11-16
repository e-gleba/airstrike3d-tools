cmake_minimum_required(VERSION 3.25)

add_library(bass_proxy SHARED bass_proxy.cpp bass_proxy.def
                                      dll_main.cpp)
target_include_directories(bass_proxy PRIVATE ${CMAKE_CURRENT_LIST_DIR})

set_target_properties(
    bass_proxy
    PROPERTIES     CXX_STANDARD 26
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
    PREFIX "" # no "lib" prefix
    )

target_link_options(
    bass_proxy
    PRIVATE
    -static
    -s)

target_link_libraries(bass_proxy PRIVATE imgui_opengl3 dwmapi glm)