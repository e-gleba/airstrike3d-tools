cpmaddpackage(
    NAME
    lua
    GIT_REPOSITORY
    https://github.com/lua/lua.git
    VERSION
    5.4.7
    DOWNLOAD_ONLY
    YES)

if(lua_ADDED)
    file(GLOB lua_sources ${lua_SOURCE_DIR}/*.c)
    list(
        REMOVE_ITEM
        lua_sources
        "${lua_SOURCE_DIR}/lua.c"
        "${lua_SOURCE_DIR}/luac.c"
        "${lua_SOURCE_DIR}/onelua.c")
    add_library(lua STATIC ${lua_sources})
    target_include_directories(lua PUBLIC $<BUILD_INTERFACE:${lua_SOURCE_DIR}>)
    # Build as C
    set_target_properties(lua PROPERTIES LINKER_LANGUAGE C)
endif()

# Sol3 via CPM
cpmaddpackage(
    NAME
    sol2
    GIT_REPOSITORY
    https://github.com/ThePhD/sol2.git
    VERSION
    3.3.0)