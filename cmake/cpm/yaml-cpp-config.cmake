cpmaddpackage(
        NAME
        yaml-cpp
        GITHUB_REPOSITORY
        jbeder/yaml-cpp
        GIT_TAG
        0.8.0
        SYSTEM
        ON
        GIT_SHALLOW
        ON
        OPTIONS
        "YAML_CPP_BUILD_TESTS OFF"
        "YAML_CPP_BUILD_TOOLS OFF"
        "YAML_CPP_BUILD_CONTRIB OFF"
        "YAML_CPP_BUILD_SHARED_LIBS OFF"
        "YAML_CPP_INSTALL OFF"
    )
