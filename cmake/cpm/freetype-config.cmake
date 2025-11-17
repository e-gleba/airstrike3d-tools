set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

cpmaddpackage(
    NAME
    freetype
    GITHUB_REPOSITORY
    aseprite/freetype2
    GIT_TAG
    VER-2-10-0
    VERSION
    2.10.0
    OPTIONS
    "FT_DISABLE_BZIP2 ON"
    "FT_DISABLE_PNG ON"
    "FT_DISABLE_HARFBUZZ ON"
    "FT_DISABLE_BROTLI ON"
    "FT_WITH_ZLIB OFF"
    "FT_ENABLE_ERROR_STRINGS OFF"
    EXCLUDE_FROM_ALL
    YES)

set_target_properties(
    freetype
    PROPERTIES CMAKE_RC_FLAGS
               "$<$<C_COMPILER_ID:MSVC>:${CMAKE_RC_FLAGS} /c65001>"
               EXCLUDE_FROM_ALL TRUE)

target_compile_options(freetype PRIVATE "$<$<C_COMPILER_ID:MSVC>:/c65001>")
target_compile_definitions(freetype PRIVATE "$<$<C_COMPILER_ID:MSVC>:_UNICODE>")

add_library(Freetype::Freetype ALIAS freetype)
set(FREETYPE_FOUND TRUE)
set(FREETYPE_INCLUDE_DIRS "")
set(FREETYPE_LIBRARIES Freetype::Freetype)