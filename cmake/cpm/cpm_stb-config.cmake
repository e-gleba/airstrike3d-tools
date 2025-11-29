# stb_image - single-file header library for image loading
# We'll use it as a header-only library

cpmaddpackage(
    NAME
    stb
    GITHUB_REPOSITORY
    nothings/stb
    GIT_TAG
    master
    DOWNLOAD_ONLY
    TRUE
)

add_library(stb INTERFACE)
target_include_directories(stb INTERFACE ${stb_SOURCE_DIR})
