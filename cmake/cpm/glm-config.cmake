cpmaddpackage(
    NAME
    glm
    GIT_REPOSITORY
    https://github.com/g-truc/glm
    GIT_TAG
    1.0.3
    OPTIONS
    "GLM_BUILD_TESTS OFF"
    "GLM_BUILD_INSTALL OFF"
    "GLM_BUILD_LIBRARY OFF"
    "GLM_ENABLE_CXX_20 ON"
    "GLM_ENABLE_LANG_EXTENSIONS ON"
    "GLM_ENABLE_SIMD_SSE4_2 ON"
    EXCLUDE_FROM_ALL
    YES
    SYSTEM
    YES)
