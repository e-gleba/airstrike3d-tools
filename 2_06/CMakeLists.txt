# 2_06/CMakeLists.txt — Airstrike3D v2.06 deployment

include("${AS3D_PROJECT_ROOT}/cmake/deploy_game.cmake")

add_game_deployment(
    VERSION       "2_06"
    GAME_EXE_NAME "as3d2.exe"
    SOURCE_DIR    "${CMAKE_CURRENT_LIST_DIR}"
)
