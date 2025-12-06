cpmaddpackage(
    NAME freetype
    GIT_REPOSITORY https://gitlab.freedesktop.org/freetype/freetype.git
    GIT_TAG VER-2-14-1
    SYSTEM ON
    GIT_SHALLOW ON
    OPTIONS
        # Architecture
        "BUILD_SHARED_LIBS OFF"
        "CMAKE_BUILD_TYPE RelWithDebInfo"
        # Hermetic Build: Force Internal/Stub Implementations
        # "Disable use of system zlib and use internal zlib library instead."
        "FT_DISABLE_ZLIB ON"
        "FT_DISABLE_BZIP2 ON"
        "FT_DISABLE_PNG ON"
        "FT_DISABLE_HARFBUZZ ON"   # Critical to prevent circular dependency
        "FT_DISABLE_BROTLI ON"
        # Bloat Removal
        "FT_ENABLE_ERROR_STRINGS OFF" # Disable verbose error strings
        "SKIP_INSTALL_ALL ON"         # Don't install targets/headers
)

if(TARGET freetype AND NOT TARGET Freetype::Freetype)
    add_library(Freetype::Freetype ALIAS freetype)
endif()
