cmake_minimum_required(VERSION 3.31...3.31)

include_guard(GLOBAL)

# cmake/toolchains/llvm_mingw.cmake
#
# Preset-driven LLVM-MinGW cross toolchain.
#
# Preset variables:
#   CMAKE_SYSTEM_PROCESSOR     x86_64 | i686 | aarch64
#   llvm_mingw_version         release tag
#   llvm_mingw_host_os         package host suffix
#   llvm_mingw_auto_download   download if missing

set(CMAKE_SYSTEM_NAME Windows)

if(NOT DEFINED CMAKE_SYSTEM_PROCESSOR OR CMAKE_SYSTEM_PROCESSOR STREQUAL "")
    set(CMAKE_SYSTEM_PROCESSOR x86_64)
endif()

set(llvm_mingw_version "20260602" CACHE STRING "llvm-mingw release tag")

set(llvm_mingw_host_os "ubuntu-22.04" CACHE STRING
                                            "llvm-mingw host OS package suffix")

set(llvm_mingw_auto_download ON CACHE BOOL "Download llvm-mingw if absent")

if(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
    set(llvm_mingw_host_arch aarch64)
else()
    set(llvm_mingw_host_arch x86_64)
endif()

if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
    set(llvm_mingw_triple aarch64-w64-mingw32)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$")
    set(llvm_mingw_triple x86_64-w64-mingw32)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(i686|i586|i486|i386)$")
    set(llvm_mingw_triple i686-w64-mingw32)
else()
    message(
        FATAL_ERROR
            "llvm-mingw: unsupported CMAKE_SYSTEM_PROCESSOR='${CMAKE_SYSTEM_PROCESSOR}'. "
            "Supported: x86_64, i686, aarch64.")
endif()

cmake_path(
    GET
    CMAKE_CURRENT_LIST_DIR
    PARENT_PATH
    llvm_mingw_cmake_dir)

cmake_path(
    GET
    llvm_mingw_cmake_dir
    PARENT_PATH
    llvm_mingw_project_root)

set(llvm_mingw_install_dir "${llvm_mingw_project_root}/llvm_mingw")

set(llvm_mingw_package_name
    "llvm-mingw-${llvm_mingw_version}-ucrt-${llvm_mingw_host_os}-${llvm_mingw_host_arch}"
    )

set(llvm_mingw_url
    "https://github.com/mstorsjo/llvm-mingw/releases/download/${llvm_mingw_version}/${llvm_mingw_package_name}.tar.xz"
    )

set(llvm_mingw_archive
    "${llvm_mingw_project_root}/${llvm_mingw_package_name}.tar.xz")

set(llvm_mingw_sysroot "${llvm_mingw_install_dir}/${llvm_mingw_triple}")

set(llvm_mingw_c_compiler
    "${llvm_mingw_install_dir}/bin/${llvm_mingw_triple}-clang")

if(NOT EXISTS "${llvm_mingw_c_compiler}")
    if(NOT llvm_mingw_auto_download)
        message(
            FATAL_ERROR
                "llvm-mingw: toolchain not found: '${llvm_mingw_install_dir}'. "
                "Enable llvm_mingw_auto_download or extract '${llvm_mingw_package_name}' manually."
            )
    endif()

    message(STATUS "llvm-mingw: downloading '${llvm_mingw_package_name}'")

    file(
        DOWNLOAD "${llvm_mingw_url}" "${llvm_mingw_archive}"
        SHOW_PROGRESS
        STATUS llvm_mingw_download_status
        TLS_VERIFY ON)

    list(
        GET
        llvm_mingw_download_status
        0
        llvm_mingw_download_code)

    if(NOT
       llvm_mingw_download_code
       EQUAL
       0)
        list(
            GET
            llvm_mingw_download_status
            1
            llvm_mingw_download_message)
        file(REMOVE "${llvm_mingw_archive}")

        message(
            FATAL_ERROR
                "llvm-mingw: download failed: ${llvm_mingw_download_message}")
    endif()

    message(STATUS "llvm-mingw: extracting '${llvm_mingw_package_name}.tar.xz'")

    file(
        ARCHIVE_EXTRACT
        INPUT
        "${llvm_mingw_archive}"
        DESTINATION
        "${llvm_mingw_project_root}")

    file(REMOVE_RECURSE "${llvm_mingw_install_dir}")

    file(RENAME "${llvm_mingw_project_root}/${llvm_mingw_package_name}"
         "${llvm_mingw_install_dir}")

    file(REMOVE "${llvm_mingw_archive}")

    message(STATUS "llvm-mingw: installed '${llvm_mingw_install_dir}'")
endif()

set(CMAKE_C_COMPILER "${llvm_mingw_install_dir}/bin/${llvm_mingw_triple}-clang"
    CACHE FILEPATH "LLVM-MinGW C compiler" FORCE)

set(CMAKE_CXX_COMPILER
    "${llvm_mingw_install_dir}/bin/${llvm_mingw_triple}-clang++"
    CACHE FILEPATH "LLVM-MinGW C++ compiler" FORCE)

set(CMAKE_RC_COMPILER
    "${llvm_mingw_install_dir}/bin/${llvm_mingw_triple}-windres"
    CACHE FILEPATH "LLVM-MinGW resource compiler" FORCE)

set(CMAKE_AR "${llvm_mingw_install_dir}/bin/llvm-ar"
    CACHE FILEPATH "LLVM archiver" FORCE)

set(CMAKE_RANLIB "${llvm_mingw_install_dir}/bin/llvm-ranlib"
    CACHE FILEPATH "LLVM ranlib" FORCE)

set(CMAKE_LINKER "${llvm_mingw_install_dir}/bin/ld.lld"
    CACHE FILEPATH "LLD linker" FORCE)

set(CMAKE_SYSROOT "${llvm_mingw_sysroot}")

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_C_FLAGS_INIT "--sysroot=${llvm_mingw_sysroot}")
set(CMAKE_CXX_FLAGS_INIT "--sysroot=${llvm_mingw_sysroot}")

set(CMAKE_EXE_LINKER_FLAGS_INIT "-fuse-ld=lld --sysroot=${llvm_mingw_sysroot}")

set(CMAKE_SHARED_LINKER_FLAGS_INIT
    "-fuse-ld=lld --sysroot=${llvm_mingw_sysroot}")

set(CMAKE_MODULE_LINKER_FLAGS_INIT
    "-fuse-ld=lld --sysroot=${llvm_mingw_sysroot}")

set(CMAKE_FIND_ROOT_PATH "${llvm_mingw_sysroot}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(CMAKE_C_CLANG_TIDY ""
    CACHE STRING "clang-tidy disabled for llvm-mingw cross builds" FORCE)

set(CMAKE_CXX_CLANG_TIDY ""
    CACHE STRING "clang-tidy disabled for llvm-mingw cross builds" FORCE)
