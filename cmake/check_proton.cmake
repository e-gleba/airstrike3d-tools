# check_proton.cmake — CTest script: verify Proton + runtime available
# Prints PROTON_SKIP if not on Linux or Steam not found → test skipped.

cmake_minimum_required(VERSION 3.31)

if(NOT CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
    message(STATUS "PROTON_SKIP — not Linux (host: ${CMAKE_HOST_SYSTEM_NAME})")
    return()
endif()

# Locate Steam root
set(steam_dirs
    "$ENV{HOME}/.var/app/com.valvesoftware.Steam/data/Steam"
    "$ENV{HOME}/.var/app/com.valvesoftware.Steam/.steam/steam"
    "$ENV{HOME}/.steam/root"
    "$ENV{HOME}/.local/share/Steam")

set(steam_root "")
foreach(p IN LISTS steam_dirs)
    if(IS_DIRECTORY "${p}")
        set(steam_root "${p}")
        break()
    endif()
endforeach()

if(NOT steam_root)
    message(STATUS "PROTON_SKIP — Steam installation not found")
    return()
endif()

set(common "${steam_root}/steamapps/common")
if(NOT IS_DIRECTORY "${common}")
    message(STATUS "PROTON_SKIP — steamapps/common missing")
    return()
endif()

# Find any Proton version
file(GLOB proton_dirs
    "${common}/Proton - Experimental"
    "${common}/GE-Proton*"
    "${common}/Proton *")

set(found "")
foreach(p IN LISTS proton_dirs)
    if(IS_DIRECTORY "${p}" AND EXISTS "${p}/proton")
        set(found "${p}")
        break()
    endif()
endforeach()

if(NOT found)
    message(STATUS "PROTON_SKIP — no Proton installation in ${common}")
    return()
endif()

message(STATUS "Proton found: ${found}")
