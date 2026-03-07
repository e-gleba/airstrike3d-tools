cmake_minimum_required(VERSION 3.21)

set(llvm_mingw_ver "20251216")
set(llvm_mingw_os "ubuntu-22.04")
set(llvm_mingw_arch
    "$<IF:$<STREQUAL:${CMAKE_HOST_SYSTEM_PROCESSOR},aarch64>,aarch64,x86_64>")

# resolve host arch at configure time (genexps not available here)
if(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
    set(host_arch "aarch64")
else()
    set(host_arch "x86_64")
endif()

set(target_prefix "${CMAKE_SYSTEM_PROCESSOR}-w64-mingw32")
set(pkg_name "llvm-mingw-${llvm_mingw_ver}-ucrt-${llvm_mingw_os}-${host_arch}")
set(toolchain_dir "${CMAKE_SOURCE_DIR}/llvm_mingw")

option(DOWNLOAD_LLVM_MINGW_IF_NOT_EXIST "auto-download llvm-mingw" ON)

message(CHECK_START "llvm-mingw")

if(NOT EXISTS "${toolchain_dir}/bin/clang")
    if(NOT DOWNLOAD_LLVM_MINGW_IF_NOT_EXIST)
        message(
            FATAL_ERROR "llvm-mingw not found at '${toolchain_dir}'"
                        " => re-run with -DDOWNLOAD_LLVM_MINGW_IF_NOT_EXIST=ON")
    endif()

    set(dl_url
        "https://github.com/mstorsjo/llvm-mingw/releases/download/${llvm_mingw_ver}/${pkg_name}.tar.xz"
        )
    set(archive "${CMAKE_SOURCE_DIR}/${pkg_name}.tar.xz")

    message(STATUS "fetching ${pkg_name}")
    file(DOWNLOAD "${dl_url}" "${archive}" SHOW_PROGRESS STATUS dl_status)
    list(
        GET
        dl_status
        0
        dl_code)
    if(NOT
       dl_code
       EQUAL
       0)
        list(
            GET
            dl_status
            1
            dl_msg)
        file(REMOVE "${archive}")
        message(FATAL_ERROR "download failed: '${dl_msg}'")
    endif()

    message(STATUS "extracting '${pkg_name}.tar.xz'")
    file(
        ARCHIVE_EXTRACT
        INPUT
        "${archive}"
        DESTINATION
        "${CMAKE_SOURCE_DIR}")
    file(REMOVE_RECURSE "${toolchain_dir}")
    file(RENAME "${CMAKE_SOURCE_DIR}/${pkg_name}" "${toolchain_dir}")
    file(REMOVE "${archive}")
endif()

message(CHECK_PASS "'${toolchain_dir}'")

set(CMAKE_C_COMPILER "${toolchain_dir}/bin/${target_prefix}-clang"
    CACHE FILEPATH "")
set(CMAKE_CXX_COMPILER "${toolchain_dir}/bin/${target_prefix}-clang++"
    CACHE FILEPATH "")
set(CMAKE_RC_COMPILER "${toolchain_dir}/bin/${target_prefix}-windres"
    CACHE FILEPATH "")
set(CMAKE_AR "${toolchain_dir}/bin/llvm-ar" CACHE FILEPATH "")
set(CMAKE_RANLIB "${toolchain_dir}/bin/llvm-ranlib" CACHE FILEPATH "")

set(CMAKE_FIND_ROOT_PATH "${toolchain_dir}/${target_prefix}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
