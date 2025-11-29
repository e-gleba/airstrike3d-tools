cpmaddpackage(
        NAME
        raylib
        GITHUB_REPOSITORY
        raysan5/raylib
        GIT_TAG
        5.0
        SYSTEM
        ON
        GIT_SHALLOW
        ON
        OPTIONS
        "BUILD_SHARED_LIBS OFF"
        "BUILD_EXAMPLES OFF"
        "BUILD_GAMES OFF"
        "USE_EXTERNAL_GLFW OFF"
        "PLATFORM Desktop"
        "SUPPORT_MODULE_RAUDIO ON"
        "CMAKE_BUILD_TYPE RelWithDebInfo"
    )
