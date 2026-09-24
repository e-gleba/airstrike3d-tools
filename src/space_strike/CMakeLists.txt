# src/space_strike/CMakeLists.txt — Space Strike deployment

include("${AS3D_PROJECT_ROOT}/cmake/deploy_game.cmake")

add_game_deployment(
    VERSION "space_strike"
    GAME_EXE_NAME "game.exe"
    SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}"
)
