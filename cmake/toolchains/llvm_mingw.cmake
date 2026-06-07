# cmake/toolchains/llvm_mingw.cmake
#
# Preset-driven LLVM-MinGW cross toolchain (CMake 3.31+)
#
# Preset variables:
#   CMAKE_SYSTEM_PROCESSOR     x86_64 | i686 | aarch64
#   llvm_mingw_version         release tag
#   llvm_mingw_host_os         package host suffix
#   llvm_mingw_auto_download   download if missing

cmake_minimum_required(VERSION 3.31...3.31)

include_guard(GLOBAL)

set(CMAKE_SYSTEM_NAME Windows)

if(NOT DEFINED CMAKE_SYSTEM_PROCESSOR OR CMAKE_SYSTEM_PROCESSOR STREQUAL "")
    set(CMAKE_SYSTEM_PROCESSOR x86_64)
endif()

set(llvm_mingw_version "20260602" CACHE STRING "llvm-mingw release tag")
set(llvm_mingw_host_os "ubuntu-22.04" CACHE STRING
                                            "llvm-mingw host OS package suffix")
set(llvm_mingw_auto_download ON CACHE BOOL "Download llvm-mingw if absent")

# Detect host architecture
if(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
    set(host_arch aarch64)
else()
    set(host_arch x86_64)
endif()

# Map target processor to triple
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
    set(triple aarch64-w64-mingw32)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$")
    set(triple x86_64-w64-mingw32)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(i686|i586|i486|i386)$")
    set(triple i686-w64-mingw32)
else()
    message(
        FATAL_ERROR
            "llvm-mingw: unsupported CMAKE_SYSTEM_PROCESSOR='${CMAKE_SYSTEM_PROCESSOR}'. "
            "Supported: x86_64, i686, aarch64.")
endif()

# Compute paths
cmake_path(
    GET
    CMAKE_CURRENT_LIST_DIR
    PARENT_PATH
    cmake_dir)
cmake_path(
    GET
    cmake_dir
    PARENT_PATH
    project_root)

cmake_path(
    APPEND
    project_root
    "llvm_mingw"
    OUTPUT_VARIABLE
    install_dir)
cmake_path(
    APPEND
    install_dir
    "bin"
    OUTPUT_VARIABLE
    bin_dir)

# Use simple string concatenation for sysroot to ensure proper variable expansion
set(sysroot "${install_dir}/${triple}")

set(package_name
    "llvm-mingw-${llvm_mingw_version}-ucrt-${llvm_mingw_host_os}-${host_arch}")
set(url
    "https://github.com/mstorsjo/llvm-mingw/releases/download/${llvm_mingw_version}/${package_name}.tar.xz"
    )

cmake_path(
    APPEND
    project_root
    "${package_name}.tar.xz"
    OUTPUT_VARIABLE
    archive)

# Compiler paths
cmake_path(
    APPEND
    bin_dir
    "${triple}-clang"
    OUTPUT_VARIABLE
    c_compiler)
cmake_path(
    APPEND
    bin_dir
    "${triple}-clang++"
    OUTPUT_VARIABLE
    cxx_compiler)
cmake_path(
    APPEND
    bin_dir
    "${triple}-windres"
    OUTPUT_VARIABLE
    rc_compiler)
cmake_path(
    APPEND
    bin_dir
    "llvm-ar"
    OUTPUT_VARIABLE
    ar_tool)
cmake_path(
    APPEND
    bin_dir
    "llvm-ranlib"
    OUTPUT_VARIABLE
    ranlib_tool)
cmake_path(
    APPEND
    bin_dir
    "ld.lld"
    OUTPUT_VARIABLE
    linker_tool)

# Auto-download if missing
if(NOT EXISTS "${c_compiler}")
    if(NOT llvm_mingw_auto_download)
        message(
            FATAL_ERROR
                "llvm-mingw: toolchain not found: '${install_dir}'. "
                "Enable llvm_mingw_auto_download or extract '${package_name}' manually."
            )
    endif()

    message(STATUS "llvm-mingw: downloading '${package_name}'")

    file(
        DOWNLOAD "${url}" "${archive}"
        SHOW_PROGRESS
        STATUS download_status
        TLS_VERIFY ON)

    list(
        GET
        download_status
        0
        download_code)
    if(NOT
       download_code
       EQUAL
       0)
        list(
            GET
            download_status
            1
            download_message)
        file(REMOVE "${archive}")
        message(FATAL_ERROR "llvm-mingw: download failed: ${download_message}")
    endif()

    message(STATUS "llvm-mingw: extracting '${package_name}.tar.xz'")

    file(
        ARCHIVE_EXTRACT
        INPUT
        "${archive}"
        DESTINATION
        "${project_root}")

    file(REMOVE_RECURSE "${install_dir}")
    cmake_path(
        APPEND
        project_root
        "${package_name}"
        OUTPUT_VARIABLE
        extracted_dir)
    file(RENAME "${extracted_dir}" "${install_dir}")
    file(REMOVE "${archive}")

    message(STATUS "llvm-mingw: installed '${install_dir}'")
endif()

# Configure toolchain
set(CMAKE_C_COMPILER "${c_compiler}" CACHE FILEPATH "LLVM-MinGW C compiler"
                                           FORCE)
set(CMAKE_CXX_COMPILER "${cxx_compiler}" CACHE FILEPATH
                                               "LLVM-MinGW C++ compiler" FORCE)
set(CMAKE_RC_COMPILER "${rc_compiler}" CACHE FILEPATH "Resource compiler" FORCE)
set(CMAKE_AR "${ar_tool}" CACHE FILEPATH "LLVM archiver" FORCE)
set(CMAKE_RANLIB "${ranlib_tool}" CACHE FILEPATH "LLVM ranlib" FORCE)
set(CMAKE_LINKER "${linker_tool}" CACHE FILEPATH "LLD linker" FORCE)

set(CMAKE_SYSROOT "${sysroot}")
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Compiler and linker flags
set(sysroot_flag "--sysroot=${sysroot}")
set(CMAKE_C_FLAGS_INIT "${sysroot_flag}")
set(CMAKE_CXX_FLAGS_INIT "${sysroot_flag}")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-fuse-ld=lld ${sysroot_flag}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-fuse-ld=lld ${sysroot_flag}")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "-fuse-ld=lld ${sysroot_flag}")

# Find paths
set(CMAKE_FIND_ROOT_PATH "${sysroot}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Disable clang-tidy for cross builds
set(CMAKE_C_CLANG_TIDY ""
    CACHE STRING "clang-tidy disabled for llvm-mingw cross builds" FORCE)
set(CMAKE_CXX_CLANG_TIDY ""
    CACHE STRING "clang-tidy disabled for llvm-mingw cross builds" FORCE)
