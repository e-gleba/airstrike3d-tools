# deploy_game.cmake — reusable game version deployment
#
#   add_game_deployment(
#       VERSION       <version_id>
#       GAME_EXE_NAME <exe_name>
#       SOURCE_DIR    <path>
#   )
#
# Deploys: data/, plugins/ (from root lua/), optional version plugins/,
# scripts/ (Python tools), bass.dll, exe, and generated config.ini.
#
# Creates targets:
#   bass_proxy_${VERSION}    — proxy DLL (if USE_BASS_PROXY_LIB)
#   deploy_game_${VERSION}   — copies runtime files to build dir
#   run_game_${VERSION}      — launches via Proton wrapper
#   install() rules          — installs to ${CMAKE_INSTALL_BINDIR}/${VERSION}
#
# When AS3D_ENABLE_TESTS is ON and PROJECT_IS_TOP_LEVEL, registers CTest
# emulator tests (deploy / proton / launch tiers) for this version.
#
# Requires the root project to define AS3D_PROJECT_ROOT.

include_guard(GLOBAL)

function(add_game_deployment)
    cmake_parse_arguments(
        PARSE_ARGV
        0
        arg
        ""
        "VERSION;GAME_EXE_NAME;SOURCE_DIR"
        "")

    if(NOT arg_VERSION OR NOT arg_GAME_EXE_NAME OR NOT arg_SOURCE_DIR)
        message(
            FATAL_ERROR
                "add_game_deployment: VERSION, GAME_EXE_NAME, SOURCE_DIR required"
            )
    endif()

    set(version "${arg_VERSION}")
    set(game_exe "${arg_GAME_EXE_NAME}")
    set(src_dir "${arg_SOURCE_DIR}")
    set(deploy_dir "${CMAKE_CURRENT_BINARY_DIR}")
    set(project_scripts_dir "${AS3D_PROJECT_ROOT}/scripts")
    set(shared_lua_dir "${AS3D_PROJECT_ROOT}/lua")
    set(version_config_in "${src_dir}/config.ini.in")

    if(NOT EXISTS "${version_config_in}")
        message(
            FATAL_ERROR
                "add_game_deployment: missing ${version_config_in}"
            )
    endif()

    cmake_path(
        APPEND
        deploy_dir
        "deploy_${version}.cmake"
        OUTPUT_VARIABLE
        deploy_script)
    cmake_path(
        APPEND
        deploy_dir
        ".deploy_stamp"
        OUTPUT_VARIABLE
        deploy_stamp)
    cmake_path(
        APPEND
        deploy_dir
        ".bass_stamp"
        OUTPUT_VARIABLE
        bass_stamp)

    # ─── Deploy script via file(CONFIGURE) ───────────────────────────────────

    file(
        CONFIGURE
        OUTPUT
        "${deploy_script}"
        CONTENT
        [=[
execute_process(
    COMMAND "@CMAKE_COMMAND@" -E make_directory "@deploy_dir@/data"
    COMMAND "@CMAKE_COMMAND@" -E copy_directory "@src_dir@/data" "@deploy_dir@/data"
)
if(EXISTS "@shared_lua_dir@")
    execute_process(
        COMMAND "@CMAKE_COMMAND@" -E make_directory "@deploy_dir@/plugins"
        COMMAND "@CMAKE_COMMAND@" -E copy_directory "@shared_lua_dir@" "@deploy_dir@/plugins"
    )
endif()
if(EXISTS "@src_dir@/plugins")
    execute_process(
        COMMAND "@CMAKE_COMMAND@" -E make_directory "@deploy_dir@/plugins"
        COMMAND "@CMAKE_COMMAND@" -E copy_directory "@src_dir@/plugins" "@deploy_dir@/plugins"
    )
endif()
if(EXISTS "@project_scripts_dir@")
    execute_process(
        COMMAND "@CMAKE_COMMAND@" -E make_directory "@deploy_dir@/scripts"
        COMMAND "@CMAKE_COMMAND@" -E copy_directory "@project_scripts_dir@" "@deploy_dir@/scripts"
    )
endif()
execute_process(
    COMMAND "@CMAKE_COMMAND@" -E copy_if_different
        "@src_dir@/@game_exe@" "@deploy_dir@/@game_exe@"
)
]=]
        @ONLY)

    file(GLOB_RECURSE shared_lua_files CONFIGURE_DEPENDS "${shared_lua_dir}/*")
    file(GLOB_RECURSE version_plugin_files CONFIGURE_DEPENDS
         "${src_dir}/plugins/*")
    file(GLOB_RECURSE project_script_files CONFIGURE_DEPENDS
         "${project_scripts_dir}/*")

    add_custom_command(
        OUTPUT "${deploy_stamp}"
        COMMENT "deploying runtime deps => ${deploy_dir}"
        COMMAND "${CMAKE_COMMAND}" -P "${deploy_script}"
        COMMAND "${CMAKE_COMMAND}" -E touch "${deploy_stamp}"
        DEPENDS
            "${src_dir}/${game_exe}"
            ${shared_lua_files}
            ${version_plugin_files}
            ${project_script_files}
            CODEGEN
        VERBATIM)

    configure_file(
        "${version_config_in}"
        "${deploy_dir}/config.ini"
        @ONLY)

    # ─── Proxy DLL ───────────────────────────────────────────────────────────

    if(USE_BASS_PROXY_LIB)
        include("${AS3D_PROJECT_ROOT}/cmake/proxy_utils.cmake")
        add_bass_proxy(
            VERSION
            "${version}"
            BASS_DLL
            "${src_dir}/bass.dll"
            SDK_TARGET
            bass_proxy_sdk)

        set(proxy_target "bass_proxy_${version}")

        add_custom_command(
            OUTPUT "${bass_stamp}"
            COMMENT "deploying ${proxy_target} + original bass.dll"
            COMMAND
                "${CMAKE_COMMAND}" -E copy_if_different "${src_dir}/bass.dll"
                "${deploy_dir}/original.dll"
            COMMAND
                "${CMAKE_COMMAND}" -E copy_if_different "${src_dir}/bass.dll"
                "${deploy_dir}/_original.dll"
            COMMAND
                "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_FILE:${proxy_target}>" "${deploy_dir}/bass.dll"
            COMMAND "${CMAKE_COMMAND}" -E touch "${bass_stamp}"
            DEPENDS "${src_dir}/bass.dll" ${proxy_target} CODEGEN
            VERBATIM)
    else()
        add_custom_command(
            OUTPUT "${bass_stamp}"
            COMMENT "copying bass.dll (no proxy)"
            COMMAND
                "${CMAKE_COMMAND}" -E copy_if_different "${src_dir}/bass.dll"
                "${deploy_dir}/bass.dll"
            COMMAND "${CMAKE_COMMAND}" -E touch "${bass_stamp}"
            DEPENDS "${src_dir}/bass.dll" CODEGEN
            VERBATIM)
    endif()

    add_custom_target(
        deploy_game_${version} ALL
        DEPENDS "${deploy_stamp}" "${bass_stamp}" "${deploy_dir}/config.ini"
        COMMENT "all runtime dependencies deployed for ${version}")

    # ─── Proton runner ───────────────────────────────────────────────────────

    configure_file(
        "${AS3D_PROJECT_ROOT}/cmake/run_with_proton.sh.in"
        "${deploy_dir}/run_game.sh"
        @ONLY
        FILE_PERMISSIONS
        OWNER_READ
        OWNER_WRITE
        OWNER_EXECUTE
        GROUP_READ
        GROUP_EXECUTE
        WORLD_READ
        WORLD_EXECUTE)

    add_custom_target(
        run_game_${version}
        COMMAND "${deploy_dir}/run_game.sh"
        WORKING_DIRECTORY "${deploy_dir}"
        COMMENT "executing run_game.sh (${version})"
        USES_TERMINAL)

    add_dependencies(run_game_${version} deploy_game_${version})

    # ─── Install ─────────────────────────────────────────────────────────────

    set(install_dest "${CMAKE_INSTALL_BINDIR}/${version}")

    install(FILES "${deploy_dir}/${game_exe}" "${deploy_dir}/run_game.sh"
                  "${deploy_dir}/bass.dll" "${deploy_dir}/config.ini"
            DESTINATION "${install_dest}")

    if(USE_BASS_PROXY_LIB)
        install(FILES "${deploy_dir}/original.dll" "${deploy_dir}/_original.dll"
                DESTINATION "${install_dest}")
    endif()

    install(DIRECTORY "${deploy_dir}/data/" DESTINATION "${install_dest}/data")

    install(DIRECTORY "${deploy_dir}/plugins/"
            DESTINATION "${install_dest}/plugins"
            OPTIONAL)

    install(DIRECTORY "${deploy_dir}/scripts/"
            DESTINATION "${install_dest}/scripts"
            OPTIONAL)

    # ─── CTest emulator tests ────────────────────────────────────────────────
    # Registered here (not in root CMakeLists.txt) because deploy_dir is
    # scope-local to this function call.  Guarded by PROJECT_IS_TOP_LEVEL
    # so subdirectory consumers don't inherit our tests.

    if(AS3D_ENABLE_TESTS AND PROJECT_IS_TOP_LEVEL)
        include("${AS3D_PROJECT_ROOT}/cmake/proton_testing.cmake")
        add_proton_emulator_tests(
            VERSION "${version}"
            GAME_EXE_NAME "${game_exe}"
            DEPLOY_DIR "${deploy_dir}"
            DEPLOY_TARGET "deploy_game_${version}")
    endif()
endfunction()
