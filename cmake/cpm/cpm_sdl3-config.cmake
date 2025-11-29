# SDL3 configuration for CPM
# Must be included before SDL3_mixer

cpmaddpackage(
        NAME
        SDL3
        GITHUB_REPOSITORY
        libsdl-org/SDL
        GIT_TAG
        a8589a84226a6202831a3d49ff4edda4acab9acd
        SYSTEM
        ON
        GIT_SHALLOW
        ON
        OPTIONS
        "SDL_STATIC ON"
        "SDL_SHARED OFF"
        "CMAKE_BUILD_TYPE RelWithDebInfo"
        "SDL_TEST_LIBRARY OFF"
        "SDL_TESTS OFF"
        "SDL_EXAMPLES OFF"
        "SDL_INSTALL_TESTS OFF"
        "SDL_DISABLE_INSTALL_DOCS ON"
        "SDL_X11
        OFF"
        "SDL_WAYLAND
        ON"
        "SDL_VULKAN
        OFF"
        "SDL_RENDER_VULKAN
        OFF"
        "SDL_ASSEMBLY
        OFF")