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
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        VERBATIM
        COMMENT "verifying .clang-tidy config in ${CMAKE_SOURCE_DIR}"
        USES_TERMINAL
    )

    file(
        GLOB_RECURSE all_sources
        CONFIGURE_DEPENDS
        "${CMAKE_SOURCE_DIR}/src/*.cpp"
    )

    # clang-tidy cant work adequately with multiple files tyring to apply all
    # fixes
    add_custom_target(clang_tidy_all)

    foreach(source_file ${all_sources})
        get_filename_component(filename ${source_file} NAME_WE)

        add_custom_command(
            TARGET clang_tidy_all
            POST_BUILD
            COMMAND
                "${clang_tidy_exe}" -p "${CMAKE_BINARY_DIR}" --fix --fix-errors
                "${source_file}"
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMENT "clang-tidy --fix --fix-errors ${filename}"
            VERBATIM
            USES_TERMINAL
        )
    endforeach()
else()
    message(
        NOTICE
        "clang-tidy not found. 'clang_tidy' target will not be available.\n"
        "install: sudo dnf install clang-tools-extra | sudo apt install clang-tidy | brew install llvm | choco install llvm"
    )
endif()