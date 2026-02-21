cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED DOWNLOAD_LLVM_MINGW_IF_NOT_EXIST)
    set(download_llvm_mingw_if_not_exist ON CACHE BOOL
                                                  "auto-download llvm-mingw")
else()
    set(download_llvm_mingw_if_not_exist "${DOWNLOAD_LLVM_MINGW_IF_NOT_EXIST}"
        CACHE BOOL "auto-download llvm-mingw")
endif()

set(llvm_mingw_ver "20251216")
set(llvm_mingw_host_os "ubuntu-22.04")

if(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
    set(host_arch "aarch64")
else()
    set(host_arch "x86_64")
endif()

set(target_prefix "${CMAKE_SYSTEM_PROCESSOR}-w64-mingw32")
set(pkg_name
    "llvm-mingw-${llvm_mingw_ver}-ucrt-${llvm_mingw_host_os}-${host_arch}")
set(dl_url
    "https://github.com/mstorsjo/llvm-mingw/releases/download/${llvm_mingw_ver}/${pkg_name}.tar.xz"
    )
set(work_dir "${CMAKE_SOURCE_DIR}")
set(archive_path "${work_dir}/${pkg_name}.tar.xz")
set(toolchain_dir "${work_dir}/llvm_mingw")

message(CHECK_START "llvm-mingw")
if(NOT EXISTS "${toolchain_dir}/bin/clang")
    if(NOT download_llvm_mingw_if_not_exist)
        message(
            FATAL_ERROR
                "== error: llvm-mingw not found at '${toolchain_dir}' => re-run with -Ddownload_llvm_mingw_if_not_exist=ON"
            )
    endif()

    message(
        STATUS
            "fetching llvm-mingw ${llvm_mingw_ver} [host=${host_arch} target=${CMAKE_SYSTEM_PROCESSOR}]..."
        )

    file(DOWNLOAD "${dl_url}" "${archive_path}" SHOW_PROGRESS STATUS dl_status)

    list(
        GET
        dl_status
        0
        dl_code)
    list(
        GET
        dl_status
        1
        dl_msg)

    if(NOT
       dl_code
       EQUAL
       0)
        file(REMOVE "${archive_path}")
        message(FATAL_ERROR "download failed: '${dl_msg}'")
    endif()

    message(STATUS "extracting '${pkg_name}.tar.xz'")
    execute_process(COMMAND ${CMAKE_COMMAND} -E tar xf "${archive_path}"
                    WORKING_DIRECTORY "${work_dir}" COMMAND_ERROR_IS_FATAL ANY)

    file(REMOVE_RECURSE "${toolchain_dir}")
    file(RENAME "${work_dir}/${pkg_name}" "${toolchain_dir}")
    file(REMOVE "${archive_path}")

    message(CHECK_PASS "downloaded '${toolchain_dir}'")
else()
    message(CHECK_PASS "'${toolchain_dir}'")
endif()

set(CMAKE_C_COMPILER "${toolchain_dir}/bin/${target_prefix}-clang"
    CACHE FILEPATH "c compiler")
set(CMAKE_CXX_COMPILER "${toolchain_dir}/bin/${target_prefix}-clang++"
    CACHE FILEPATH "c++ compiler")
set(CMAKE_RC_COMPILER "${toolchain_dir}/bin/${target_prefix}-windres"
    CACHE FILEPATH "resource compiler")
set(CMAKE_AR "${toolchain_dir}/bin/llvm-ar" CACHE FILEPATH "archiver")
set(CMAKE_RANLIB "${toolchain_dir}/bin/llvm-ranlib" CACHE FILEPATH "ranlib")

set(CMAKE_FIND_ROOT_PATH "${toolchain_dir}/${target_prefix}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)