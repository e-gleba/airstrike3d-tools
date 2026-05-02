function(generate_proxy_def input_dll output_var)
    # toolchain auto-downloads to llvm_mingw (underscore);
    # local setup may use llvm-mingw (hyphen) per README instructions.
    # Use NO_DEFAULT_PATH so we never silently fall back to GNU objdump
    # in /usr/bin — it cannot parse PE exports.
    find_program(
        objdump_bin
        NAMES llvm-objdump objdump
        HINTS
            "${CMAKE_SOURCE_DIR}/llvm_mingw/bin"
            "${CMAKE_SOURCE_DIR}/llvm-mingw/bin"
        NO_DEFAULT_PATH
        REQUIRED)

    cmake_path(
        GET
        input_dll
        STEM
        LAST_ONLY
        dll_stem) # LAST_ONLY: safe for dotted paths
    set(def_file "${CMAKE_CURRENT_BINARY_DIR}/${dll_stem}.def")

    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
                                           "${input_dll}")

    message(STATUS "parsing exports '${objdump_bin}': ${input_dll}")

    execute_process(
        COMMAND "${objdump_bin}" -p "${input_dll}"
        OUTPUT_VARIABLE dump_output
        OUTPUT_STRIP_TRAILING_WHITESPACE
        COMMAND_ERROR_IS_FATAL
        ANY
        COMMAND_ECHO
        STDOUT)

    string(
        REGEX MATCHALL
              "[0-9]+[ \t]+0x[0-9a-fA-F]+[ \t]+[^ \t\r\n]+"
              raw_exports
              "${dump_output}")

    list(LENGTH raw_exports export_count)
    if(export_count EQUAL 0)
        message(FATAL_ERROR "generate_proxy_def: no exports in '${input_dll}'")
    endif()

    set(content "LIBRARY \"${dll_stem}.dll\"\nEXPORTS\n")

    foreach(row IN LISTS raw_exports)
        string(
            REGEX
            REPLACE "([0-9]+)[ \t]+0x[0-9a-fA-F]+[ \t]+([^ \t\r\n]+)"
                    "\\2"
                    sym_name
                    "${row}")
        string(
            REGEX
            REPLACE "([0-9]+)[ \t]+0x[0-9a-fA-F]+[ \t]+([^ \t\r\n]+)"
                    "\\1"
                    ordinal
                    "${row}")
        string(APPEND content
               "    ${sym_name}=original.${sym_name} @${ordinal}\n")
    endforeach()

    file(WRITE "${def_file}" "${content}")
    message(STATUS "generated '${def_file}' (${export_count} exports)")

    set(${output_var} "${def_file}")
    return(PROPAGATE ${output_var})
endfunction()

generate_proxy_def("${CMAKE_SOURCE_DIR}/2_06/bass.dll" bass_proxy_definitions)

file(
    GLOB_RECURSE
    sdk_sources
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/*.cpp")

add_library(bass_proxy SHARED ${bass_proxy_definitions} ${sdk_sources})
target_include_directories(bass_proxy PRIVATE "${CMAKE_CURRENT_LIST_DIR}")

target_compile_features(bass_proxy PRIVATE cxx_std_26)

set_target_properties(bass_proxy PROPERTIES CXX_EXTENSIONS OFF PREFIX "")

target_compile_definitions(
    bass_proxy
    PRIVATE WIN32_LEAN_AND_MEAN
            NOMINMAX
            GLM_FORCE_RADIANS
            GLM_FORCE_DEPTH_ZERO_TO_ONE)

target_link_options(
    bass_proxy
    PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-static>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-s>)

target_link_libraries(
    bass_proxy
    PRIVATE warnings
            imgui_opengl3
            dwmapi
            glm
            glu32
            opengl32
            safetyhook
            spdlog::spdlog
            sol2::sol2
            lua)
