cpmaddpackage(
    NAME sol2
    GITHUB_REPOSITORY ThePhD/sol2
    GIT_TAG main
    SYSTEM ON
    GIT_SHALLOW ON
    OPTIONS
        # Lua Dependency Management
        "SOL2_BUILD_LUA ON"        # Force internal build of Lua (Hermetic)
        "SOL2_LUA_VERSION 5.4.4"   # Pin the version
        "BUILD_LUA_AS_DLL OFF"     # Force Static Lua (matches your "less code" static preference)

        # Bloat Removal
        "SOL2_TESTS OFF"
        "SOL2_EXAMPLES OFF"
        "SOL2_INTEROP_EXAMPLES OFF"
        "SOL2_DYNAMIC_LOADING_EXAMPLES OFF"
        "SOL2_DOCS OFF"
        
        # Single Header Generation (Kill it)
        # You don't need the 10MB single header if you are using CMake properly.
        "SOL2_SINGLE OFF"
        "SOL2_TESTS_SINGLE OFF"
        "SOL2_EXAMPLES_SINGLE OFF"
        
        # Installation
        "SOL2_ENABLE_INSTALL OFF"  # Don't install to system
)
