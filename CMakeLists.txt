cmake_minimum_required(VERSION 3.31)

project(
    as3d_reverse_toolkit
    VERSION 1.0.6
    DESCRIPTION "Airstrike divo games reverse engineering toolkit"
    HOMEPAGE_URL "https://github.com/e-gleba/airstrike3d-tools"
    LANGUAGES C CXX)

include(CMakePrintHelpers)

option(USE_BASS_PROXY_LIB "Build the BASS proxy DLL" ON)

list(
    APPEND
    CMAKE_PREFIX_PATH
    ${CMAKE_CURRENT_LIST_DIR}/cmake
    ${CMAKE_CURRENT_LIST_DIR}/cmake/code_quality)

list(
    APPEND
    CMAKE_FIND_ROOT_PATH
    ${CMAKE_CURRENT_LIST_DIR}/cmake
    ${CMAKE_CURRENT_LIST_DIR}/cmake/code-quality)

cmake_print_variables(
    CMAKE_PREFIX_PATH
    CMAKE_FIND_ROOT_PATH
    CMAKE_CROSSCOMPILING
    CMAKE_SYSTEM_NAME
    CMAKE_CXX_COMPILER_ID
    CMAKE_CXX_COMPILER_VERSION)

find_package(warnings CONFIG REQUIRED)

if(PROJECT_IS_TOP_LEVEL)
    find_package(clang_format CONFIG REQUIRED)
    find_package(clang_tidy CONFIG REQUIRED)
    find_package(clang_doc CONFIG REQUIRED)
endif()

add_subdirectory(external SYSTEM)
find_package(cpm CONFIG REQUIRED)

add_subdirectory(src)
add_subdirectory(2_06)

include(CPack)
