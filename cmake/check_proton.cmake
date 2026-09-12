# check_proton.cmake — CTest script: verify Proton + runtime available
# Prints PROTON_SKIP if not on Linux or Steam not found → test skipped.
# Mirrors the discovery paths of cmake/run_with_proton.sh.in.

cmake_minimum_required(VERSION 3.31)

if(NOT CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
    message(STATUS "PROTON_SKIP — not Linux (host: ${CMAKE_HOST_SYSTEM_NAME})")
    return()
endif()

# Locate Steam root (env override wins, same as the launcher)
set(steam_root "$ENV{STEAM_ROOT}")
if(NOT steam_root OR NOT IS_DIRECTORY "${steam_root}")
    set(steam_dirs
        "$ENV{HOME}/.var/app/com.valvesoftware.Steam/data/Steam"
        "$ENV{HOME}/.var/app/com.valvesoftware.Steam/.steam/steam"
        "$ENV{HOME}/.steam/root"
        "$ENV{HOME}/.steam/steam"
        "$ENV{HOME}/.local/share/Steam")

    set(steam_root "")
    foreach(p IN LISTS steam_dirs)
        if(IS_DIRECTORY "${p}")
            set(steam_root "${p}")
            break()
        endif()
    endforeach()
endif()

if(NOT steam_root)
    message(STATUS "PROTON_SKIP — Steam installation not found")
    return()
endif()

set(common "${steam_root}/steamapps/common")
set(compat_dirs
    "${steam_root}/compatibilitytools.d"
    "$ENV{HOME}/.local/share/Steam/compatibilitytools.d"
    "$ENV{HOME}/.steam/root/compatibilitytools.d")

# Find any Proton toolkit (env override wins, same as the launcher)
set(found "")
if(DEFINED ENV{PROTON_PATH} AND EXISTS "$ENV{PROTON_PATH}/proton")
    set(found "$ENV{PROTON_PATH}")
else()
    file(
        GLOB proton_dirs
        "${common}/Proton - Experimental"
        "${common}/GE-Proton*"
        "${common}/Proton *"
        "${steam_root}/compatibilitytools.d/Proton - Experimental"
        "${steam_root}/compatibilitytools.d/GE-Proton*"
        "${steam_root}/compatibilitytools.d/Proton *")
    foreach(p IN LISTS proton_dirs compat_dirs)
        if(IS_DIRECTORY "${p}" AND EXISTS "${p}/proton")
            set(found "${p}")
            break()
        endif()
    endforeach()
endif()

if(NOT found)
    message(STATUS "PROTON_SKIP — no Proton installation (Steam > Settings > Compatibility; GE-Proton goes to ${steam_root}/compatibilitytools.d)")
    return()
endif()

message(STATUS "Proton found: ${found}")

# Warn (don't fail) when no container runtime is present — late-game crashes
# and garbled text are typical without one.
set(runtime_override "$ENV{STEAM_RUNTIME_PATH}")
if(runtime_override)
    if(NOT EXISTS "${runtime_override}/_v2-entry-point")
        message(WARNING "STEAM_RUNTIME_PATH invalid: ${runtime_override}/_v2-entry-point missing")
    endif()
else()
    file(
        GLOB runtime_dirs
        "${common}/SteamLinuxRuntime_sniper"
        "${common}/SteamLinuxRuntime_soldier"
        "${common}/SteamLinuxRuntime_scout"
        "${common}/SteamLinuxRuntime*")
    set(runtime_found FALSE)
    foreach(p IN LISTS runtime_dirs)
        if(IS_DIRECTORY "${p}" AND EXISTS "${p}/_v2-entry-point")
            set(runtime_found TRUE)
            break()
        endif()
    endforeach()
    if(NOT runtime_found)
        message(
            WARNING
            "No Steam Linux Runtime in ${common} — emulator launches directly; install 'Steam Linux Runtime 3.0 (sniper)' via Steam > Library > Tools"
        )
    endif()
endif()
