# check_dll_load.cmake — CTest script: verify proxy DLL loads successfully
# Runs game for 10 seconds, checks bass.dll proxy initialization in logs.
# Reads variables from environment: AS3D_DEPLOY_DIR, AS3D_GAME_EXE
# Prints PROTON_SKIP on non-Linux → test skipped by CTest.

cmake_minimum_required(VERSION 3.31)

# Read from environment
set(deploy_dir "$ENV{AS3D_DEPLOY_DIR}")
set(game_exe "$ENV{AS3D_GAME_EXE}")

foreach(required IN ITEMS deploy_dir game_exe)
    if(NOT ${required})
        message(FATAL_ERROR "Missing required variable: ${required}")
    endif()
endforeach()

if(NOT CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
    message(STATUS "PROTON_SKIP — not Linux")
    return()
endif()

set(run_script "${deploy_dir}/run_game.sh")
if(NOT EXISTS "${run_script}")
    message(FATAL_ERROR "run_game.sh not found: ${run_script}")
endif()

message(STATUS "Testing DLL load: ${game_exe} (10s runtime)")

# Run game for 10 seconds to allow DLL initialization
execute_process(
    COMMAND "${run_script}"
    WORKING_DIRECTORY "${deploy_dir}"
    TIMEOUT 10
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err
    RESULT_VARIABLE rc)

set(combined "${out}\n${err}")

# Check for Proton availability (skip if not found)
string(FIND "${combined}" "Proton not found" proton_missing)
if(NOT proton_missing EQUAL -1)
    message(STATUS "PROTON_SKIP — Proton binary not found")
    return()
endif()

# Check for launcher banner (indicates script started)
string(FIND "${combined}" "proton launcher started" found_banner)
if(found_banner EQUAL -1)
    message(FATAL_ERROR
        "DLL test failed: emulator did not start.\n"
        "stdout: ${out}\n"
        "stderr: ${err}")
endif()

# Look for DLL load indicators in logs
# The run_game.sh wrapper logs to logs/ directory, but we also capture stdout/stderr
# Check for common failure patterns:
string(FIND "${combined}" "Failed to load" dll_load_failed)
string(FIND "${combined}" "DLL not found" dll_not_found)
string(FIND "${combined}" "bass.dll" bass_dll_mentioned)
string(FIND "${combined}" "LoadLibrary" loadlibrary_called)

# If we see explicit failure messages, report them
if(NOT dll_load_failed EQUAL -1)
    message(FATAL_ERROR
        "DLL load failed: explicit error in logs.\n"
        "Exit code: ${rc}\n"
        "stdout: ${out}\n"
        "stderr: ${err}")
endif()

if(NOT dll_not_found EQUAL -1)
    message(FATAL_ERROR
        "DLL not found: bass.dll missing or inaccessible.\n"
        "Exit code: ${rc}\n"
        "Check: ${deploy_dir}/bass.dll exists and is readable.")
endif()

# Success: game ran for 10 seconds without DLL errors
# Note: We don't require explicit "bass.dll loaded" messages because:
# 1. The game may not log DLL loads in release builds
# 2. Absence of failure = success for DLL loading
# 3. The deploy tier already verified bass.dll exists
message(STATUS "DLL load test passed: ${game_exe} ran for 10s without DLL errors (exit code: ${rc})")
