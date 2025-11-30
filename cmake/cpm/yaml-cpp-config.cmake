set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

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
        # Architecture
        "YAML_BUILD_SHARED_LIBS OFF"       # Core option for static build
        "BUILD_SHARED_LIBS OFF"
        "YAML_ENABLE_PIC ON"               # Good for linking into other libs
        
        # Bloat Removal
        "YAML_CPP_BUILD_TESTS OFF"
        "YAML_CPP_BUILD_TOOLS OFF"         # Kills the CLI tools
        "YAML_CPP_BUILD_CONTRIB OFF"       # Kills experimental/contrib stuff
        "YAML_CPP_INSTALL OFF"             # No system install
        "YAML_CPP_FORMAT_SOURCE OFF"       # Don't run clang-format during build
)
