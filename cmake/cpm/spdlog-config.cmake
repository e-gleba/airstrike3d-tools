cpmaddpackage(
    NAME
    spdlog
    VERSION
    1.17.0
    GIT_REPOSITORY
    https://github.com/gabime/spdlog
    GIT_TAG
    v1.17.0
    EXCLUDE_FROM_ALL
    YES
    SYSTEM
    YES
    OPTIONS
    "SPDLOG_BUILD_EXAMPLE OFF" # Skip examples
    "SPDLOG_BUILD_TESTS OFF" # Skip tests
    "SPDLOG_BUILD_BENCH OFF" # Skip benchmarks
    "SPDLOG_BUILD_SHARED OFF" # Build as static library
    "SPDLOG_INSTALL OFF" # Skip install targets
    "SPDLOG_FUZZ OFF" # Skip fuzzing
    "SPDLOG_USE_STD_FORMAT YES" # Use C++20 std::format (no fmt dependency)
    "SPDLOG_SYSTEM_INCLUDES YES" # Mark as system headers (suppress warnings)
    )
