# proton_testing.cmake — CTest integration for Proton launcher emulator
#
#   add_proton_emulator_tests(
#       VERSION       <version_id>
#       GAME_EXE_NAME <exe_name>
#       DEPLOY_DIR    <path>
#       DEPLOY_TARGET <target>
#   )
#
# Registers CTest tests that validate the cross-compiler tuning emulator
# (Proton launcher wrapper) for a given game version.
#
# Test tiers:
#   deploy   — verify all runtime files staged correctly
#   proton   — verify Proton + Steam Linux Runtime discovery
#   launch   — short-lived emulator smoke test (5s timeout)
#   dll_load — verify proxy DLL loads without errors (10s timeout)
#
# Requires: CMake 3.31+, deploy_game.cmake already configured.

include_guard(GLOBAL)

# ─── Public API ────────────────────────────────────────────────────────────────

function(add_proton_emulator_tests)
    cmake_parse_arguments(
        PARSE_ARGV
        0
        arg
        ""
        "VERSION;GAME_EXE_NAME;DEPLOY_DIR;DEPLOY_TARGET"
        "")

    foreach(required IN ITEMS VERSION GAME_EXE_NAME DEPLOY_DIR DEPLOY_TARGET)
        if(NOT arg_${required})
            message(FATAL_ERROR "add_proton_emulator_tests: ${required} required")
        endif()
    endforeach()

    set(ver ${arg_VERSION})
    set(exe ${arg_GAME_EXE_NAME})
    set(dir ${arg_DEPLOY_DIR})
    set(tgt ${arg_DEPLOY_TARGET})

    # CMAKE_CURRENT_FUNCTION_LIST_DIR = directory where this function was
    # defined (cmake/), regardless of where it's called from (2_06/, etc.)
    set(test_scripts_dir "${CMAKE_CURRENT_FUNCTION_LIST_DIR}")

    # ── Tier 1: Deployment validation ────────────────────────────────────────
    # Fast, no-Proton-required. Runs on any platform.

    add_test(
        NAME "deploy_files_exist_${ver}"
        COMMAND
            "${CMAKE_COMMAND}" -P "${test_scripts_dir}/check_deploy.cmake")
    set_tests_properties(
        "deploy_files_exist_${ver}"
        PROPERTIES
            LABELS "deploy;${ver}"
            FIXTURES_REQUIRED "deploy_${ver}"
            TIMEOUT 10
            ENVIRONMENT "AS3D_DEPLOY_DIR=${dir};AS3D_GAME_EXE=${exe}")

    # Fixture: deployment must complete before deploy-tests run
    add_test(
        NAME "deploy_fixture_${ver}"
        COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}"
                --target "${tgt}" --config "$<CONFIG>")
    set_tests_properties(
        "deploy_fixture_${ver}"
        PROPERTIES
            LABELS "deploy;fixture;${ver}"
            FIXTURES_SETUP "deploy_${ver}"
            TIMEOUT 300)

    # ── Tier 2: Proton availability (Linux-only, skipped elsewhere) ──────────

    add_test(
        NAME "proton_available_${ver}"
        COMMAND
            "${CMAKE_COMMAND}" -P "${test_scripts_dir}/check_proton.cmake")
    set_tests_properties(
        "proton_available_${ver}"
        PROPERTIES
            LABELS "proton;integration;${ver}"
            FIXTURES_REQUIRED "deploy_${ver}"
            SKIP_REGULAR_EXPRESSION "PROTON_SKIP"
            TIMEOUT 15
            ENVIRONMENT "AS3D_DEPLOY_DIR=${dir}")

    # ── Tier 3: Emulator smoke launch ────────────────────────────────────────
    # Runs run_game.sh, expects clean banner output within timeout.
    # Game will likely hang waiting for input — we just verify it boots.

    add_test(
        NAME "emulator_launch_${ver}"
        COMMAND
            "${CMAKE_COMMAND}" -P "${test_scripts_dir}/check_launch.cmake")
    set_tests_properties(
        "emulator_launch_${ver}"
        PROPERTIES
            LABELS "launch;integration;${ver}"
            FIXTURES_REQUIRED "deploy_${ver};proton_available_${ver}"
            SKIP_REGULAR_EXPRESSION "PROTON_SKIP"
            TIMEOUT 30
            ENVIRONMENT "AS3D_DEPLOY_DIR=${dir};AS3D_GAME_EXE=${exe};AS3D_TEST_TIMEOUT=${AS3D_EMULATOR_TEST_TIMEOUT}")

    # ── Tier 4: DLL load validation ─────────────────────────────────────────
    # Runs game for 10 seconds, verifies bass.dll proxy loads without errors.
    # Catches runtime DLL issues (missing dependencies, initialization failures).

    add_test(
        NAME "dll_load_${ver}"
        COMMAND
            "${CMAKE_COMMAND}" -P "${test_scripts_dir}/check_dll_load.cmake")
    set_tests_properties(
        "dll_load_${ver}"
        PROPERTIES
            LABELS "dll_load;integration;${ver}"
            FIXTURES_REQUIRED "deploy_${ver};proton_available_${ver}"
            SKIP_REGULAR_EXPRESSION "PROTON_SKIP"
            TIMEOUT 30
            ENVIRONMENT "AS3D_DEPLOY_DIR=${dir};AS3D_GAME_EXE=${exe}")

    message(STATUS "Registered Proton emulator tests for ${ver}")
endfunction()
