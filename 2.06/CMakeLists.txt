add_custom_target(
    deploy_game ALL
    COMMENT "deploying runtime deps => ${CMAKE_CURRENT_BINARY_DIR}")

set(game_exe_name "as3d2.exe")

add_custom_command(
    TARGET deploy_game
    PRE_BUILD
    COMMAND
        ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_LIST_DIR}/data"
        "${CMAKE_CURRENT_BINARY_DIR}/data"
    COMMAND
        ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_LIST_DIR}/${game_exe_name}"
        "${CMAKE_CURRENT_BINARY_DIR}/${game_exe_name}")

configure_file(
    "${CMAKE_CURRENT_LIST_DIR}/run_with_proton.sh.in"
    "${CMAKE_CURRENT_BINARY_DIR}/run_game.sh"
    @ONLY
    FILE_PERMISSIONS
    OWNER_READ
    OWNER_WRITE
    OWNER_EXECUTE
    GROUP_READ
    GROUP_EXECUTE
    WORLD_READ
    WORLD_EXECUTE)

if(NOT TARGET bass_proxy)
    add_custom_command(
        TARGET deploy_game
        PRE_BUILD
        COMMAND
            ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_LIST_DIR}/bass.dll"
            "${CMAKE_CURRENT_BINARY_DIR}/bass.dll")
else()
    add_custom_command(
        TARGET deploy_game
        PRE_BUILD
        COMMAND
            ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_LIST_DIR}/bass.dll"
            "${CMAKE_CURRENT_BINARY_DIR}/original.dll"
        COMMAND
            ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_LIST_DIR}/bass.dll"
            "${CMAKE_CURRENT_BINARY_DIR}/_original.dll"
        COMMAND
            ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:bass_proxy>"
            "${CMAKE_CURRENT_BINARY_DIR}/bass.dll")
endif()

install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}" DESTINATION 2.06)
include(CPack)