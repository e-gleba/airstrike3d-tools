# check_launch.cmake — CTest script: smoke-test emulator launch
# Runs run_game.sh with short timeout, verifies banner output appears.
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

execute_process(
    COMMAND "${run_script}"
    WORKING_DIRECTORY "${deploy_dir}"
    TIMEOUT "${timeout_seconds}"
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err
    RESULT_VARIABLE rc)

# Combine output for search
set(combined "${out}\n${err}")

# Success indicator: banner line from run_with_proton.sh
string(FIND "${combined}" "proton launcher started" found_banner)
if(found_banner EQUAL -1)
    message(FATAL_ERROR
        "Emulator failed to produce banner within ${timeout_seconds}s.\n"
        "stdout: ${out}\n"
        "stderr: ${err}")
endif()

# Check Proton was discovered (not just bash starting)
string(FIND "${combined}" "Proton not found" proton_missing)
if(NOT proton_missing EQUAL -1)
    message(STATUS "PROTON_SKIP — Proton binary not found")
    return()
endif()

message(STATUS "Emulator smoke test passed for ${game_exe}")
