# Direct3D 8 is not part of the modern Windows SDK. MinGW-w64 ships the
# headers/import lib; MSVC-ABI toolchains (clang --target=*-windows-msvc,
# cl.exe) need a fallback SDK for compile + link.

find_path(AS3D_D3D8_INCLUDE_DIR NAMES d3d8.h)

if(AS3D_D3D8_INCLUDE_DIR)
    message(STATUS "Found system Direct3D 8 headers: ${AS3D_D3D8_INCLUDE_DIR}")
    return()
endif()

set(_as3d_needs_vendored_d3d8 FALSE)
if(MSVC)
    set(_as3d_needs_vendored_d3d8 TRUE)
elseif(DEFINED CMAKE_CXX_COMPILER_TARGET
       AND CMAKE_CXX_COMPILER_TARGET MATCHES "windows-msvc")
    set(_as3d_needs_vendored_d3d8 TRUE)
endif()

if(NOT _as3d_needs_vendored_d3d8)
    message(
        FATAL_ERROR
            "d3d8.h was not found. MinGW-w64 toolchains should provide it via "
            "the sysroot; MSVC-ABI builds fetch a minimal DirectX 8 SDK.")
endif()

cpmaddpackage(
    NAME
    min_dx8_sdk
    GIT_REPOSITORY
    https://github.com/TheSuperHackers/min-dx8-sdk.git
    GIT_TAG
    7bddff8c01f5fb931c3cb73d4aa8e66d303d97bc
    DOWNLOAD_ONLY
    YES)

set(_as3d_d3d8_include "${min_dx8_sdk_SOURCE_DIR}")
set(_as3d_d3d8_lib "${min_dx8_sdk_SOURCE_DIR}/d3d8.lib")

if(NOT EXISTS "${_as3d_d3d8_include}/d3d8.h")
    message(FATAL_ERROR "min-dx8-sdk is missing d3d8.h at ${_as3d_d3d8_include}")
endif()
if(NOT EXISTS "${_as3d_d3d8_lib}")
    message(FATAL_ERROR "min-dx8-sdk is missing d3d8.lib at ${_as3d_d3d8_lib}")
endif()

add_library(d3d8 UNKNOWN IMPORTED GLOBAL)
set_target_properties(
    d3d8
    PROPERTIES IMPORTED_LOCATION "${_as3d_d3d8_lib}"
               INTERFACE_INCLUDE_DIRECTORIES "${_as3d_d3d8_include}")

message(STATUS "Using vendored Direct3D 8 SDK from min-dx8-sdk")
