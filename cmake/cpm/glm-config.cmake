cpmaddpackage(
    NAME glm
    GITHUB_REPOSITORY g-truc/glm
    GIT_TAG 1.0.2
    SYSTEM ON
    GIT_SHALLOW ON
    OPTIONS
        # Core
        "GLM_BUILD_LIBRARY OFF"       # Header-only is cleaner
        "GLM_BUILD_TESTS OFF"
        "GLM_BUILD_INSTALL OFF"
        
        # Modern C++ & Performance
        "GLM_ENABLE_CXX_20 ON"        # Matches your "modern" pref
        "GLM_ENABLE_LANG_EXTENSIONS ON"
        "GLM_ENABLE_FAST_MATH ON"     # -ffast-math / /fp:fast
        "GLM_ENABLE_SIMD_AVX2 ON"     # Explicitly request SIMD if target supports it (optional)
)
