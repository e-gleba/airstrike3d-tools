cmake_minimum_required(VERSION 3.28)

add_custom_target(
    deploy_symlink ALL
    COMMENT "symlinking runtime deps => ${CMAKE_CURRENT_BINARY_DIR}")

add_custom_command(
    TARGET deploy_symlink
    PRE_BUILD
    COMMAND
        ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_LIST_DIR}/data"
        "${CMAKE_CURRENT_BINARY_DIR}/data"
    COMMAND
        ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_LIST_DIR}/as3d2.exe"
        "${CMAKE_CURRENT_BINARY_DIR}/as3d2.exe")

if(NOT TARGET bass_proxy)
    add_custom_command(
        TARGET deploy_symlink
        PRE_BUILD
        COMMAND
            ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_LIST_DIR}/bass.dll"
            "${CMAKE_CURRENT_BINARY_DIR}/bass.dll")
else()
    add_custom_command(
        TARGET deploy_symlink
        PRE_BUILD
        COMMAND
            ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_LIST_DIR}/bass.dll"
            "${CMAKE_CURRENT_BINARY_DIR}/bass_real.dll"
        COMMAND
            ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_LIST_DIR}/bass.dll"
            "${CMAKE_CURRENT_BINARY_DIR}/_bass_real.dll"
        COMMAND
            ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:bass_proxy>"
            "${CMAKE_CURRENT_BINARY_DIR}/bass.dll")
endif()