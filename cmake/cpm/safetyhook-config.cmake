cpmaddpackage(
    NAME
    safetyhook
    VERSION
    0.7.0
    GIT_REPOSITORY
    https://github.com/cursey/safetyhook
    GIT_TAG
    v0.7.0
    EXCLUDE_FROM_ALL
    YES
    SYSTEM
    YES
    OPTIONS
    "SAFETYHOOK_FETCH_ZYDIS YES" # Fetch Zydis disassembler (required)
    "SAFETYHOOK_BUILD_TEST OFF" # Skip tests
    "SAFETYHOOK_BUILD_EXAMPLES OFF" # Skip examples
    "CMAKE_UNITY_BUILD OFF")
