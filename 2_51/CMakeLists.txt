# 2_51/CMakeLists.txt — Airstrike3D v2.51 deployment

include("${CMAKE_SOURCE_DIR}/cmake/deploy_game.cmake")

add_game_deployment(
    VERSION       "2_51"
    GAME_EXE_NAME "as3d2.exe"
    SOURCE_DIR    "${CMAKE_CURRENT_LIST_DIR}"
)
