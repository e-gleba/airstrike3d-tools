# 2_71/CMakeLists.txt — Airstrike3D II Gulf v2.71 deployment

include("${CMAKE_SOURCE_DIR}/cmake/deploy_game.cmake")

add_game_deployment(
    VERSION
    "2_71"
    GAME_EXE_NAME
    "as3d_gulf.exe"
    SOURCE_DIR
    "${CMAKE_CURRENT_LIST_DIR}")
