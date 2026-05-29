# 2_06/CMakeLists.txt — Airstrike3D v2.06 deployment
#
# Expects the parent scope to set:
#   GAME_VERSION   — e.g. "2_06"
#   GAME_EXE_NAME  — e.g. "as3d2.exe"
#
# Produces:
#   bass_proxy_${GAME_VERSION}   — proxy DLL (if USE_BASS_PROXY_LIB)
#   deploy_game_${GAME_VERSION}  — copies runtime files to build dir
#   run_game_${GAME_VERSION}     — launches via Proton wrapper
#   install()                    — installs to ${CMAKE_INSTALL_BINDIR}/${GAME_VERSION}

set(game_exe_name "${GAME_EXE_NAME}")
set(deploy_dir "${CMAKE_CURRENT_BINARY_DIR}")
set(source_dir "${CMAKE_CURRENT_LIST_DIR}")
set(deploy_stamp "${deploy_dir}/.deploy_stamp")

# Build the deploy command piecemeal — data/ and plugins/ may not exist.
set(deploy_cmd
    "${CMAKE_COMMAND}" -E make_directory "${deploy_dir}/data"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${source_dir}/data" "${deploy_dir}/data"
)

if(EXISTS "${source_dir}/plugins")
    list(
        APPEND
        deploy_cmd
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${deploy_dir}/plugins"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory "${source_dir}/plugins" "${deploy_dir}/plugins"
    )
endif()

list(
    APPEND
    deploy_cmd
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${source_dir}/${game_exe_name}" "${deploy_dir}/${game_exe_name}"
    COMMAND "${CMAKE_COMMAND}" -E touch "${deploy_stamp}"
)

add_custom_command(
    OUTPUT "${deploy_stamp}"
    COMMENT "deploying runtime deps => ${deploy_dir}"
    ${deploy_cmd}
    DEPENDS "${source_dir}/${game_exe_name}"
    VERBATIM)

# ─── Proxy DLL ────────────────────────────────────────────────────────────────

set(bass_stamp "${deploy_dir}/.bass_stamp")

if(USE_BASS_PROXY_LIB)
    include("${CMAKE_SOURCE_DIR}/cmake/proxy_utils.cmake")
    add_bass_proxy(
        VERSION "${GAME_VERSION}"
        BASS_DLL "${source_dir}/bass.dll"
        SDK_TARGET bass_proxy_sdk)

    set(proxy_target "bass_proxy_${GAME_VERSION}")

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
            "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:${proxy_target}>"
            "${deploy_dir}/bass.dll"
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
    deploy_game_${GAME_VERSION} ALL
    DEPENDS "${deploy_stamp}" "${bass_stamp}"
    COMMENT "all runtime dependencies deployed for ${GAME_VERSION}")

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
    run_game_${GAME_VERSION}
    COMMAND "${deploy_dir}/run_game.sh"
    WORKING_DIRECTORY "${deploy_dir}"
    COMMENT "executing script run_game.sh (${GAME_VERSION})"
    USES_TERMINAL)

add_dependencies(run_game_${GAME_VERSION} deploy_game_${GAME_VERSION})

# ─── Install — runtime-only, no build artifacts ───────────────────────────────

install(
    FILES "${deploy_dir}/${game_exe_name}"
          "${deploy_dir}/run_game.sh"
    TYPE BIN
    COMPONENT "${GAME_VERSION}"
    RENAME "${game_exe_name}")

install(
    FILES "${deploy_dir}/bass.dll"
    TYPE BIN
    COMPONENT "${GAME_VERSION}")

if(USE_BASS_PROXY_LIB)
    install(
        FILES "${deploy_dir}/original.dll"
              "${deploy_dir}/_original.dll"
        TYPE BIN
        COMPONENT "${GAME_VERSION}")
endif()

install(
    DIRECTORY "${deploy_dir}/data/"
    TYPE BIN
    COMPONENT "${GAME_VERSION}"
    FILES_MATCHING
    PATTERN "*")

if(EXISTS "${deploy_dir}/plugins")
    install(
        DIRECTORY "${deploy_dir}/plugins/"
        TYPE BIN
        COMPONENT "${GAME_VERSION}"
        FILES_MATCHING
        PATTERN "*")
endif()
