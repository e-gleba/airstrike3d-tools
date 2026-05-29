# proxy_utils.cmake — reusable helpers for DLL proxy generation
#
# Provides:
#   generate_proxy_def(input_dll output_var)
#       Parses exports from a Windows DLL using llvm-objdump/objdump and
#       writes a .def module-definition file that forwards every export
#       through an "original." prefix.
#
#   add_versioned_proxy(VERSION <v> BASS_DLL <path>)
#       Creates a SHARED library target bass_proxy_<v> that links against
#       the bass_proxy_sdk OBJECT library.  The .def is generated from the
#       version-specific bass.dll so each proxy matches its runtime.

include_guard(GLOBAL)

# ─── generate_proxy_def ───────────────────────────────────────────────────────
# Parse the export table of a PE DLL and emit a .def file that forwards every
# exported symbol through an "original." prefix, preserving ordinals.
#
#   generate_proxy_def(
#       input_dll   # absolute path to the bass.dll for this version
#       output_var  # name of variable to hold the generated .def path
#   )
#
# The .def is written to CMAKE_CURRENT_BINARY_DIR/<dll_stem>.def.
# A directory-level configure dependency on input_dll is registered so that
# the build system re-generates if the DLL changes.
function(generate_proxy_def input_dll output_var)
    find_program(
        objdump_bin
        NAMES llvm-objdump objdump
        HINTS "${CMAKE_SOURCE_DIR}/llvm_mingw/bin"
        REQUIRED)

    cmake_path(GET input_dll STEM LAST_ONLY dll_stem)
    set(def_file "${CMAKE_CURRENT_BINARY_DIR}/${dll_stem}.def")

    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
                                       "${input_dll}")

    message(STATUS "parsing exports '${objdump_bin}': ${input_dll}")

    execute_process(
        COMMAND "${objdump_bin}" -p "${input_dll}"
        OUTPUT_VARIABLE dump_output
        OUTPUT_STRIP_TRAILING_WHITESPACE
        COMMAND_ERROR_IS_FATAL ANY
        COMMAND_ECHO STDOUT)

    string(
        REGEX MATCHALL
              "[0-9]+[ \t]+0x[0-9a-fA-F]+[ \t]+[^ \t\r\n]+"
              raw_exports
              "${dump_output}")

    list(LENGTH raw_exports export_count)
    if(export_count EQUAL 0)
        message(
            FATAL_ERROR
                "generate_proxy_def: no exports found in '${input_dll}'")
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

# ─── add_versioned_proxy ──────────────────────────────────────────────────────
# Create a SHARED library bass_proxy_<VERSION> that:
#   1. Generates a .def from <BASS_DLL>
#   2. Links bass_proxy_sdk (the OBJECT library containing all hook/overlay code)
#   3. Applies all required link libraries and options
#
# The caller (src/proxy/CMakeLists.txt) must have already created the
# bass_proxy_sdk OBJECT library.
function(add_versioned_proxy)
    set(options "")
    set(one_value_args VERSION BASS_DLL)
    set(multi_value_args "")
    cmake_parse_arguments(PARSE_ARGV 0 ARG "${options}" "${one_value_args}"
                          "${multi_value_args}")

    if(NOT ARG_VERSION OR NOT ARG_BASS_DLL)
        message(
            FATAL_ERROR
                "add_versioned_proxy: VERSION and BASS_DLL are required")
    endif()

    if(NOT TARGET bass_proxy_sdk)
        message(
            FATAL_ERROR
                "add_versioned_proxy: bass_proxy_sdk OBJECT library not found. "
                "Ensure src/proxy/CMakeLists.txt creates it before calling this function."
        )
    endif()

    generate_proxy_def("${ARG_BASS_DLL}" def_file)

    set(target_name "bass_proxy_${ARG_VERSION}")

    add_library(${target_name} SHARED "${def_file}")

    target_link_libraries(${target_name} PRIVATE bass_proxy_sdk)

    set_target_properties(
        ${target_name}
        PROPERTIES CXX_EXTENSIONS OFF
                   PREFIX ""
                   OUTPUT_NAME "bass")

    target_compile_definitions(${target_name}
                               PRIVATE "BASS_VERSION=${ARG_VERSION}")

    target_link_options(
        ${target_name}
        PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-static>
        $<$<CXX_COMPILER_ID:GNU,Clang>:-s>)

    target_link_libraries(
        ${target_name}
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

    message(
        STATUS
            "created proxy target '${target_name}' (bass: ${ARG_BASS_DLL}, ${export_count} exports)"
    )
endfunction()
