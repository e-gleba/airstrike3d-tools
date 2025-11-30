cpmaddpackage(
    NAME entt
    GITHUB_REPOSITORY skypjack/entt
    GIT_TAG v3.16.0
    SYSTEM ON
    GIT_SHALLOW ON
    OPTIONS
        # Core
        "ENTT_USE_LIBCPP OFF"          # Let your toolchain decide (usually OFF unless you force libc++ on Linux)
        "ENTT_USE_SANITIZER OFF"
        "ENTT_USE_CLANG_TIDY OFF"
        
        # Bloat Removal
        "ENTT_BUILD_TESTING OFF"
        "ENTT_BUILD_TESTBED OFF"
        "ENTT_BUILD_DOCS OFF"
        "ENTT_BUILD_BENCHMARK OFF"
        "ENTT_BUILD_EXAMPLE OFF"
        "ENTT_BUILD_LIB OFF"
        "ENTT_BUILD_SNAPSHOT OFF"
        
        # Installation
        "ENTT_INSTALL OFF"             # No system install
        "ENTT_INCLUDE_HEADERS OFF"     # CMake handles includes via target interface, no need to list files
        "ENTT_INCLUDE_NATVIS OFF"      # Only helpful for VS debug, OFF for cleaner build
)
