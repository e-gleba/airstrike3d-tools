cpmaddpackage(
    NAME
    freetype2_upstream
    GIT_REPOSITORY
    https://github.com/freetype/freetype
    GIT_TAG
    VER-2-14-3
    OPTIONS
    "FT_DISABLE_ZLIB ON"
    "FT_DISABLE_BZIP2 ON"
    "FT_DISABLE_PNG ON"
    "FT_DISABLE_HARFBUZZ ON"
    "FT_DISABLE_BROTLI ON"
    "BUILD_SHARED_LIBS OFF"
    "CMAKE_UNITY_BUILD OFF"
    EXCLUDE_FROM_ALL
    YES
    SYSTEM
    YES)