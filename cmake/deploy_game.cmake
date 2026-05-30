# deploy_game.cmake — reusable game version deployment
#
#   add_game_deployment(
#       VERSION       <version_id>    # e.g. "2_06"
#       GAME_EXE_NAME <exe_name>      # e.g. "as3d2.exe"
#       SOURCE_DIR    <path>          # source directory for this version
#   )
#
# Creates targets:
#   bass_proxy_${VERSION}    — proxy DLL (if USE_BASS_PROXY_LIB)
#   deploy_game_${VERSION}   — copies runtime files to build dir
#   run_game_${VERSION}      — launches via Proton wrapper
#   install() rules          — installs to ${CMAKE_INSTALL_BINDIR}/${VERSION}

include_guard(GLOBAL)

function(add_game_deployment)
    set(options "")
    set(one_value_args VERSION GAME_EXE_NAME SOURCE_DIR)
    set(multi_value_args "")
    cmake_parse_arguments(PARSE_ARGV 0 ARG "${options}" "${one_value_args}" "${multi_value_args}")

    if(NOT ARG_VERSION OR NOT ARG_GAME_EXE_NAME OR NOT ARG_SOURCE_DIR)
        message(
            FATAL_ERROR
            "add_game_deployment: VERSION, GAME_EXE_NAME, SOURCE_DIR are required"
        )
    endif()

    set(game_exe_name "${ARG_GAME_EXE_NAME}")
    set(deploy_dir "${CMAKE_CURRENT_BINARY_DIR}")
    set(source_dir "${ARG_SOURCE_DIR}")
    set(deploy_stamp "${deploy_dir}/.deploy_stamp")

    # ─── Deploy script (generated via string(CONFIGURE)) ──────────────────────

    set(deploy_script "${deploy_dir}/deploy_${ARG_VERSION}.cmake")

    set(deploy_script_content
        [=[
# Auto-generated deploy script for @ARG_VERSION@
set(source_dir "@source_dir@")
set(deploy_dir "@deploy_dir@")
set(game_exe_name "@game_exe_name@")

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${deploy_dir}/data"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${source_dir}/data" "${deploy_dir}/data"
)

if(EXISTS "${source_dir}/plugins")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${deploy_dir}/plugins"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory "${source_dir}/plugins" "${deploy_dir}/plugins"
    )
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${source_dir}/${game_exe_name}" "${deploy_dir}/${game_exe_name}"
)
]=])

    string(CONFIGURE "${deploy_script_content}" deploy_script_content @ONLY)
    file(WRITE "${deploy_script}" "${deploy_script_content}")

    add_custom_command(
        OUTPUT "${deploy_stamp}"
        COMMENT "deploying runtime deps => ${deploy_dir}"
        COMMAND "${CMAKE_COMMAND}" -P "${deploy_script}"
        COMMAND "${CMAKE_COMMAND}" -E touch "${deploy_stamp}"
        DEPENDS "${source_dir}/${game_exe_name}"
        VERBATIM)

    # ─── Proxy DLL ────────────────────────────────────────────────────────────

    set(bass_stamp "${deploy_dir}/.bass_stamp")

    if(USE_BASS_PROXY_LIB)
        include("${CMAKE_SOURCE_DIR}/cmake/proxy_utils.cmake")
        add_bass_proxy(
            VERSION "${ARG_VERSION}"
            BASS_DLL "${source_dir}/bass.dll"
            SDK_TARGET bass_proxy_sdk)

        set(proxy_target "bass_proxy_${ARG_VERSION}")

        add_custom_command(
            OUTPUT "${bass_stamp}"
            COMMENT "deploying ${proxy_target} + original bass.dll"
            COMMAND
                "${CMAKE_COMMAND}" -E copy_if_different "${source_dir}/bass.dll"
                "${deploy_dir}/original.dll"
            COMMAND
                "${CMAKE_COMMAND}" -E copy_if_different "${source_dir}/bass.dll"
                "${deploy_dir}/_original.dll"
            COMMAND
                "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_FILE:${proxy_target}>" "${deploy_dir}/bass.dll"
            COMMAND "${CMAKE_COMMAND}" -E touch "${bass_stamp}"
            DEPENDS "${source_dir}/bass.dll" ${proxy_target}
            VERBATIM)
    else()
        add_custom_command(
            OUTPUT "${bass_stamp}"
            COMMENT "copying bass.dll (no proxy)"
            COMMAND
                "${CMAKE_COMMAND}" -E copy_if_different "${source_dir}/bass.dll"
                "${deploy_dir}/bass.dll"
            COMMAND "${CMAKE_COMMAND}" -E touch "${bass_stamp}"
            DEPENDS "${source_dir}/bass.dll"
            VERBATIM)
    endif()

    add_custom_target(
        deploy_game_${ARG_VERSION} ALL DEPENDS "${deploy_stamp}" "${bass_stamp}"
        COMMENT "all runtime dependencies deployed for ${ARG_VERSION}")

    # ─── Proton runner script ─────────────────────────────────────────────────

    configure_file(
        "${CMAKE_SOURCE_DIR}/cmake/run_with_proton.sh.in"
        "${deploy_dir}/run_game.sh"
        @ONLY
        FILE_PERMISSIONS
        OWNER_READ OWNER_WRITE OWNER_EXECUTE
        GROUP_READ GROUP_EXECUTE
        WORLD_READ WORLD_EXECUTE)

    add_custom_target(
        run_game_${ARG_VERSION}
        COMMAND "${deploy_dir}/run_game.sh"
        WORKING_DIRECTORY "${deploy_dir}"
        COMMENT "executing run_game.sh (${ARG_VERSION})"
        USES_TERMINAL)

    add_dependencies(run_game_${ARG_VERSION} deploy_game_${ARG_VERSION})

    # ─── Install — separate directory per version ─────────────────────────────

    set(install_dest "${CMAKE_INSTALL_BINDIR}/${ARG_VERSION}")

    install(FILES "${deploy_dir}/${game_exe_name}" DESTINATION "${install_dest}")
    install(FILES "${deploy_dir}/run_game.sh" DESTINATION "${install_dest}")
    install(FILES "${deploy_dir}/bass.dll" DESTINATION "${install_dest}")

    if(USE_BASS_PROXY_LIB)
        install(
            FILES "${deploy_dir}/original.dll" "${deploy_dir}/_original.dll"
            DESTINATION "${install_dest}")
    endif()

    install(
        DIRECTORY "${deploy_dir}/data/"
        DESTINATION "${install_dest}/data"
        FILES_MATCHING
        PATTERN "*")

    if(EXISTS "${deploy_dir}/plugins")
        install(
            DIRECTORY "${deploy_dir}/plugins/"
            DESTINATION "${install_dest}/plugins"
            FILES_MATCHING
            PATTERN "*")
    endif()
endfunction()
