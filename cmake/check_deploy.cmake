# check_deploy.cmake — CTest script: verify deployment artifacts exist
# Invoked via: cmake -P check_deploy.cmake -Ddeploy_dir=... -Dgame_exe=...

cmake_minimum_required(VERSION 3.31)

foreach(required IN ITEMS deploy_dir game_exe)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing required variable: ${required}")
    endif()
endforeach()

set(expected_files
    "${deploy_dir}/${game_exe}"
    "${deploy_dir}/bass.dll"
    "${deploy_dir}/run_game.sh")

if(USE_BASS_PROXY_LIB)
    list(APPEND expected_files
        "${deploy_dir}/original.dll"
        "${deploy_dir}/_original.dll")
endif()

set(missing "")
foreach(f IN LISTS expected_files)
    if(NOT EXISTS "${f}")
        list(APPEND missing "${f}")
    endif()
endforeach()

if(NOT IS_DIRECTORY "${deploy_dir}/data")
    list(APPEND missing "${deploy_dir}/data/")
endif()

if(missing)
    list(JOIN missing "\n  " report)
    message(FATAL_ERROR "Deployment incomplete. Missing:\n  ${report}")
endif()

message(STATUS "All deployment artifacts present for ${deploy_dir}")
