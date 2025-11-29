cpmaddpackage(
    NAME tinyobjloader
    GITHUB_REPOSITORY tinyobjloader/tinyobjloader
    GIT_TAG release # or a fixed version/tag, e.g. v1.0.6
    OPTIONS
        "TINYOBJLOADER_BUILD_SHARED_LIBS OFF"
        "BUILD_SHARED_LIBS OFF"
        "TINYOBJLOADER_BUILD_TESTS OFF"
        "TINYOBJLOADER_BUILD_EXAMPLES OFF"
)
