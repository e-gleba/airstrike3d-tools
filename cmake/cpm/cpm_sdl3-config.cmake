cpmaddpackage(
    NAME SDL3
    GITHUB_REPOSITORY libsdl-org/SDL
    GIT_TAG main
    SYSTEM ON
    GIT_SHALLOW ON
    OPTIONS
        # Force Static Build
        "SDL_STATIC ON"
        "SDL_SHARED OFF"
        "BUILD_SHARED_LIBS OFF"
        
        # Minimal Core
        "SDL_TEST_LIBRARY OFF"
        "SDL_TESTS OFF"
        "SDL_EXAMPLES OFF"
        "SDL_INSTALL_TESTS OFF"
        "SDL_DISABLE_INSTALL_DOCS ON"
        
        # Subsystems (Explicitly Enabled/Disabled)
        # Only enable what you actually use. 
        # Assuming a modern Linux desktop context based on your history.
        
        "SDL_VIDEO ON"
        "SDL_RENDER ON" # Needed for 2D API or if using SDL_Renderer
        "SDL_GPU ON"    # You mentioned using SDL_GPU
        "SDL_AUDIO ON"  # Needed for Mixer
        "SDL_EVENTS ON"
        
        # Backend Specifics (Linux)
        "SDL_X11 ON"
        "SDL_X11_XSCRNSAVER OFF"  # Optional, not always available
        "SDL_X11_XTEST OFF"       # Optional, not always available
        "SDL_WAYLAND ON"       # Modern Linux default
        "SDL_VULKAN ON"        # Critical for SDL_GPU
        "SDL_RENDER_VULKAN ON" 
        "SDL_OPENGL ON"        # If you still use GL legacy
        
        # Disable unnecessary subsystems/drivers to reduce bloat
        "SDL_HAPTIC OFF"
        "SDL_POWER OFF"
        "SDL_SENSOR OFF"
        "SDL_DIALOG OFF"
        "SDL_TRAY OFF"
        "SDL_CAMERA OFF" 
        "SDL_HIDAPI OFF"       # Unless you need complex controller support
        "SDL_JOYSTICK OFF"     # Unless you need gamepads
        
        # Audio Drivers (Keep only what's needed)
        "SDL_ALSA ON"          # Standard Linux audio
        "SDL_PULSEAUDIO ON"
        "SDL_PIPEWIRE ON"
        "SDL_JACK OFF"
        "SDL_SNDIO OFF"
        "SDL_OSS OFF"
        
        # Advanced
        "SDL_ASSEMBLY ON"      # Keep optimizations
        "SDL_LIBC ON"          # Use system libc (fclose errors and etc)
        "SDL_DEPS_SHARED OFF"  # Link deps statically if possible
)
