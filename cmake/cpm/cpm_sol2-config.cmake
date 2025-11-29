cpmaddpackage(
    NAME sol2
    GITHUB_REPOSITORY ThePhD/sol2
    GIT_TAG main
    OPTIONS
        "SOL2_BUILD_LUA ON"        # Build Lua automatically
        "SOL2_LUA_VERSION 5.4.4"   # Use Lua 5.4.4
        "SOL2_TESTS OFF"
        "SOL2_EXAMPLES OFF"
        "SOL2_SINGLE OFF"
        "SOL2_DOCS OFF"
)
