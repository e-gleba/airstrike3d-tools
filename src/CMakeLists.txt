# Shared include directory for both engine and game
set(SHARED_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shared PARENT_SCOPE)
set(SHARED_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shared)

add_subdirectory(engine)
add_subdirectory(game)
