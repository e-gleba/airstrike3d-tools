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

    # ── Tier 1: Deployment validation ────────────────────────────────────────
    # Fast, no-Proton-required. Runs on any platform.

    add_test(
        NAME "deploy_files_exist_${ver}"
        COMMAND
            "${CMAKE_COMMAND}" -E
            # cmake -E doesn't have "assert_exists", so we use a tiny script
            "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_LIST_DIR}/check_deploy.cmake"
            "-Ddeploy_dir=${dir}"
            "-Dgame_exe=${exe}")
    set_tests_properties(
        "deploy_files_exist_${ver}"
        PROPERTIES
            LABELS "deploy;${ver}"
            FIXTURES_REQUIRED "deploy_${ver}"
            TIMEOUT 10)

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
            "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_LIST_DIR}/check_proton.cmake"
            "-Ddeploy_dir=${dir}")
    set_tests_properties(
        "proton_available_${ver}"
        PROPERTIES
            LABELS "proton;integration;${ver}"
            FIXTURES_REQUIRED "deploy_${ver}"
            SKIP_REGULAR_EXPRESSION "PROTON_SKIP"
            TIMEOUT 15)

    # ── Tier 3: Emulator smoke launch ────────────────────────────────────────
    # Runs run_game.sh, expects clean banner output within timeout.
    # Game will likely hang waiting for input — we just verify it boots.

    add_test(
        NAME "emulator_launch_${ver}"
        COMMAND
            "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_LIST_DIR}/check_launch.cmake"
            "-Ddeploy_dir=${dir}"
            "-Dgame_exe=${exe}"
            "-Dtimeout_seconds=${AS3D_EMULATOR_TEST_TIMEOUT}")
    set_tests_properties(
        "emulator_launch_${ver}"
        PROPERTIES
            LABELS "launch;integration;${ver}"
            FIXTURES_REQUIRED "deploy_${ver};proton_available_${ver}"
            SKIP_REGULAR_EXPRESSION "PROTON_SKIP"
            TIMEOUT 30
            ENVIRONMENT "AS3D_TEST_VERSION=${ver}")

    message(STATUS "Registered Proton emulator tests for ${ver}")
endfunction()
