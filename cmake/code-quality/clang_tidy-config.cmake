find_program(
    clang_tidy_exe
    NAMES clang-tidy
    DOC
        "clang-tidy: clang-based C++ linter. Install: 'sudo dnf install clang-tools-extra', 'sudo apt install clang-tidy', 'brew install llvm', or 'choco install llvm'. Required for 'clang_tidy' target."
)

if(clang_tidy_exe)
    add_custom_target(
        clang_tidy_verify_config
        COMMAND "${clang_tidy_exe}" --verify-config
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        VERBATIM
        COMMENT "verifying .clang-tidy config"
        USES_TERMINAL
    )

    file(
        GLOB_RECURSE all_sources
        CONFIGURE_DEPENDS
        "${PROJECT_SOURCE_DIR}/src/*.cpp"
        "${PROJECT_SOURCE_DIR}/src/*.hpp"
    )

    add_custom_target(
        clang_tidy
        COMMAND "${clang_tidy_exe}" --fix --fix-errors ${all_sources}
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        VERBATIM
        COMMENT "running clang-tidy with auto-fix on all sources"
        USES_TERMINAL
    )
else()
    message(
        NOTICE
        "clang-tidy not found. 'clang_tidy' target will not be available.\n"
        "install: sudo dnf install clang-tools-extra | sudo apt install clang-tidy | brew install llvm | choco install llvm"
    )
endif()

# example to enable globaly:
# set(CMAKE_CXX_CLANG_TIDY clang-tidy -checks=-*,readability-*)
