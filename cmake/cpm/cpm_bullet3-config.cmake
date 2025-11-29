cpmaddpackage(
    NAME bullet3
    GITHUB_REPOSITORY bulletphysics/bullet3
    GIT_TAG master
    OPTIONS
        "BUILD_BULLET2_DEMOS OFF"
        "BUILD_BULLET3 ON"
        "BUILD_CPU_DEMOS OFF"
        "BUILD_OPENGL3_DEMOS OFF"
        "BUILD_UNIT_TESTS OFF"
        "BUILD_EXTRAS OFF"
        "BUILD_PYBULLET OFF"
        "BUILD_ENET OFF"
        "BUILD_CLSOCKET OFF"
        "INSTALL_LIBS OFF"
        "USE_GRAPHICAL_BENCHMARK OFF"
)