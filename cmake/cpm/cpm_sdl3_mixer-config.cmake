set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

cpmaddpackage(
        NAME
        SDL3_mixer
        GITHUB_REPOSITORY
        libsdl-org/SDL_mixer
        GIT_TAG
        main
        SYSTEM
        ON
        GIT_SHALLOW
        ON
        OPTIONS
        "MIX_WAV ON"
        "MIX_OGG ON"
        "MIX_MP3 ON"
        "MIX_FLAC OFF"
        "MIX_MOD OFF"
        "MIX_MIDI OFF"
        "MIX_OPUS OFF"
        "SDL3MIXER_BUILD_SHARED OFF"
        "SDL3MIXER_BUILD_STATIC ON"
        "SDL3MIXER_SAMPLES OFF"
        "SDL3MIXER_TESTS OFF"
    )