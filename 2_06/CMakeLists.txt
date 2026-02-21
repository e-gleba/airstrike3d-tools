set(game_exe_name "as3d2.exe")
set(deploy_dir "${CMAKE_CURRENT_BINARY_DIR}")
set(source_dir "${CMAKE_CURRENT_LIST_DIR}")

set(deploy_stamp "${deploy_dir}/.deploy_stamp")

add_custom_command(
    OUTPUT "${deploy_stamp}"
    COMMENT "Deploying runtime deps => ${deploy_dir}"
    # Copy data directory
    COMMAND
        ${CMAKE_COMMAND} -E copy_directory "${source_dir}/data"
        "${deploy_dir}/data"
    # Copy plugins directory
    COMMAND
        ${CMAKE_COMMAND} -E copy_directory "${source_dir}/plugins"
        "${deploy_dir}/plugins"
    # Copy game executable
    COMMAND
        ${CMAKE_COMMAND} -E copy_if_different "${source_dir}/${game_exe_name}"
        "${deploy_dir}/${game_exe_name}"
    # Touch a stamp file so CMake can track freshness
    COMMAND ${CMAKE_COMMAND} -E touch "${deploy_stamp}"
    DEPENDS "${source_dir}/${game_exe_name}"
    VERBATIM)

set(bass_stamp "${deploy_dir}/.bass_stamp")

if(NOT TARGET bass_proxy)
    # No proxy — just copy bass.dll as-is
    add_custom_command(
        OUTPUT "${bass_stamp}"
        COMMENT "Copying bass.dll (no proxy)"
        COMMAND
            ${CMAKE_COMMAND} -E copy_if_different "${source_dir}/bass.dll"
            "${deploy_dir}/bass.dll"
        COMMAND ${CMAKE_COMMAND} -E touch "${bass_stamp}"
        DEPENDS "${source_dir}/bass.dll"
        VERBATIM)
else()
    # Proxy build — rename original, copy proxy as bass.dll
    add_custom_command(
        OUTPUT "${bass_stamp}"
        COMMENT "Deploying bass_proxy + original bass.dll"
        COMMAND
            ${CMAKE_COMMAND} -E copy_if_different "${source_dir}/bass.dll"
            "${deploy_dir}/original.dll"
        COMMAND
            ${CMAKE_COMMAND} -E copy_if_different "${source_dir}/bass.dll"
            "${deploy_dir}/_original.dll"
        COMMAND
            ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:bass_proxy>"
            "${deploy_dir}/bass.dll"
        COMMAND ${CMAKE_COMMAND} -E touch "${bass_stamp}"
        DEPENDS "${source_dir}/bass.dll" bass_proxy
        VERBATIM)
endif()

add_custom_target(deploy_game ALL DEPENDS "${deploy_stamp}" "${bass_stamp}"
                  COMMENT "All runtime dependencies deployed")

configure_file(
    "${source_dir}/run_with_proton.sh.in"
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
    run_game
    COMMAND "${deploy_dir}/run_game.sh"
    WORKING_DIRECTORY "${deploy_dir}"
    COMMENT "Executing script => run_game.sh"
    USES_TERMINAL)

add_dependencies(run_game deploy_game)

install(DIRECTORY "${deploy_dir}/" DESTINATION 2_06)
include(CPack)
