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
    YES
)

if(stb_ADDED)
    add_library(stb INTERFACE)
    add_library(stb::stb ALIAS stb) # Create namespace alias for consistency

    target_include_directories(stb SYSTEM INTERFACE ${stb_SOURCE_DIR})
endif()
