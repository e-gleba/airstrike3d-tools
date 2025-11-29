cpmaddpackage(
    NAME SDL3
    GITHUB_REPOSITORY libsdl-org/SDL
    GIT_TAG main
    SYSTEM ON
    GIT_SHALLOW ON
    OPTIONS
        "SDL_STATIC ON"
        "SDL_SHARED OFF"
        "CMAKE_BUILD_TYPE RelWithDebInfo"
        "SDL_TEST_LIBRARY OFF"
        "SDL_TESTS OFF"
        "SDL_EXAMPLES OFF"
        "SDL_INSTALL_TESTS OFF"
        "SDL_DISABLE_INSTALL_DOCS ON"
        "SDL_X11_XSCRNSAVER OFF"
        "SDL_X11_XTEST OFF"
        "SDL_X11 ON"           
        "SDL_WAYLAND ON"       
        "SDL_VULKAN ON"        
        "SDL_RENDER_VULKAN ON" 
        "SDL_ASSEMBLY OFF"
)
