cpmaddpackage(
    NAME
    stb
    GITHUB_REPOSITORY
    nothings/stb
    SYSTEM
    ON
    GIT_SHALLOW
    ON
    GIT_TAG
    master
    DOWNLOAD_ONLY
    TRUE)

add_library(stb INTERFACE)
target_include_directories(stb SYSTEM INTERFACE ${stb_SOURCE_DIR})
