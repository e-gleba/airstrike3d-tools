# check_launch.cmake — CTest script: smoke-test emulator launch + DLL load
# Runs game for 10 seconds, verifies:
# 1. Banner appears in logs (Proton started)
# 2. Process exits cleanly (exit code 0 or timeout)
# 3. No DLL load errors in output
#
# Reads variables from environment: AS3D_DEPLOY_DIR, AS3D_GAME_EXE, AS3D_TEST_TIMEOUT
# Prints PROTON_SKIP on non-Linux → test skipped by CTest.

cmake_minimum_required(VERSION 3.31)

# Read from environment
set(deploy_dir "$ENV{AS3D_DEPLOY_DIR}")
set(game_exe "$ENV{AS3D_GAME_EXE}")
set(timeout_seconds "$ENV{AS3D_TEST_TIMEOUT}")

foreach(required IN ITEMS deploy_dir game_exe timeout_seconds)
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

message(STATUS "Launching emulator: ${run_script} (timeout: ${timeout_seconds}s)")

# Run game with timeout
execute_process(
    COMMAND "${run_script}"
    WORKING_DIRECTORY "${deploy_dir}"
    TIMEOUT "${timeout_seconds}"
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
        "Emulator did not start (no banner in output).\n"
        "Exit code: ${rc}\n"
        "stdout: ${out}\n"
        "stderr: ${err}")
endif()

# Check process exit code
# Exit code 0 = clean exit (game ran and exited)
# Exit code from timeout = game was still running (expected, not an error)
# Any other exit code = process crashed or failed to start
if(rc EQUAL 0)
    message(STATUS "Emulator exited cleanly for ${game_exe}")
elseif(rc EQUAL 124)
    # timeout(1) returns 124 when process is killed due to timeout
    message(STATUS "Emulator timed out after ${timeout_seconds}s (expected for long-running game)")
else()
    # Any other exit code indicates failure (crash, DLL load error, etc.)
    message(FATAL_ERROR
        "Emulator failed with exit code ${rc}.\n"
        "This typically indicates:\n"
        "  - DLL load failure (bass.dll or dependencies)\n"
        "  - Game crash during initialization\n"
        "  - Missing runtime dependencies\n"
        "stdout: ${out}\n"
        "stderr: ${err}")
endif()

# Additional check: look for explicit DLL errors in output
string(FIND "${combined}" "Failed to load" dll_error)
if(NOT dll_error EQUAL -1)
    message(FATAL_ERROR
        "DLL load error detected in output.\n"
        "Exit code: ${rc}\n"
        "stdout: ${out}\n"
        "stderr: ${err}")
endif()

message(STATUS "Emulator smoke test passed for ${game_exe} (exit code: ${rc})")
