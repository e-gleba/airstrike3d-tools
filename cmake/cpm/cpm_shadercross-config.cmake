cpmaddpackage(
    NAME SDL_shadercross
    GITHUB_REPOSITORY libsdl-org/SDL_shadercross
    GIT_TAG main
    OPTIONS
        # Важнейшая опция: скачать и собрать зависимости (spirv-cross, dxxc) внутри
        "SDLSHADERCROSS_VENDORED ON" 
        "SDLSHADERCROSS_SHARED OFF"
        "SDLSHADERCROSS_STATIC ON"
        # Если нужен только CLI tool для оффлайн компиляции:
        "SDLSHADERCROSS_CLI ON"
)
