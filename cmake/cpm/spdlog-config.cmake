cpmaddpackage(
    NAME spdlog
    GITHUB_REPOSITORY gabime/spdlog
    GIT_TAG v1.16.0 # Consider v1.15.0 if 1.16 isn't out yet, check tags
    SYSTEM ON
    GIT_SHALLOW ON
    OPTIONS
        # Modern C++20 Integration
        "SPDLOG_USE_STD_FORMAT ON"      # Use std::format (requires C++20 compliant compiler)
        
        # Bloat Removal
        "SPDLOG_BUILD_EXAMPLE OFF"
        "SPDLOG_BUILD_TESTS OFF"
        "SPDLOG_BUILD_BENCH OFF"
        "SPDLOG_FUZZ OFF"
        "SPDLOG_INSTALL OFF"            # Don't install to system
        
        # Build Architecture
        "SPDLOG_FMT_EXTERNAL OFF"       # We use std::format, so no fmt lib needed
        "SPDLOG_BUILD_SHARED OFF"       # Static build
        "SPDLOG_SYSTEM_INCLUDES ON"     # Suppress warnings from spdlog headers
        "CMAKE_POSITION_INDEPENDENT_CODE ON"  # Required for shared library linking
)
