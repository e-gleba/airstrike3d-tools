cpmaddpackage(
    NAME bullet3
    GITHUB_REPOSITORY bulletphysics/bullet3
    GIT_TAG master
    SYSTEM ON
    GIT_SHALLOW ON
    OPTIONS
        # Architecture
        "BUILD_SHARED_LIBS OFF"
        "USE_DOUBLE_PRECISION OFF"       # Keep float (standard for games), set ON if you need high-precision sim
        "CMAKE_BUILD_TYPE RelWithDebInfo"

        # Core Components
        "BUILD_BULLET3 ON"               # Modern Bullet3 API (GPU-friendly structures)
        
        # Bloat Removal - Demos & Tests
        "BUILD_BULLET2_DEMOS OFF"
        "BUILD_CPU_DEMOS OFF"
        "BUILD_OPENGL3_DEMOS OFF"
        "BUILD_UNIT_TESTS OFF"
        "USE_GRAPHICAL_BENCHMARK OFF"    # Kills the GUI benchmark app
        
        # Bloat Removal - Bindings & Extras
        "BUILD_PYBULLET OFF"
        "BUILD_EXTRAS OFF"               # Kills importers/exporters you likely don't need yet
        "BUILD_ENET OFF"                 # No networking
        "BUILD_CLSOCKET OFF"             # No networking
        
        # Installation
        "INSTALL_LIBS OFF"               # Don't pollute system paths
        "INSTALL_CMAKE_FILES OFF"        # Don't install config files
        
        # Threading (Optional - Enable if you have heavy physics)
        "BULLET2_MULTITHREADING ON"      # Safe to enable for modern C++
)
