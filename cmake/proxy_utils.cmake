# proxy_utils.cmake — reusable helpers for DLL proxy generation
#
# Provides:
#   generate_proxy_def(input_dll output_var)
#       Parses exports from a Windows DLL using llvm-objdump/objdump and
#       writes a .def module-definition file that forwards every export
#       through an "original." prefix.
#
#   add_bass_proxy(VERSION <v> BASS_DLL <path> SDK_TARGET <tgt>)
#       Creates a SHARED library bass_proxy_<v> in the caller's directory
#       scope.  Links <SDK_TARGET> (usually bass_proxy_sdk).  The .def is
#       generated from the version-specific bass.dll so each proxy matches
#       its runtime export table.
#
#       Also generates a version_info stub so the SDK can display the
#       version string at runtime without recompiling the static lib.
#
#       Output lands in CMAKE_CURRENT_BINARY_DIR (unique per 2_XX/), so
#       multiple versions never collide.

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

# ─── add_bass_proxy ───────────────────────────────────────────────────────────
# Create a SHARED library bass_proxy_<VERSION> in the caller's directory scope.
#
#   add_bass_proxy(
#       VERSION    <version_id>    # e.g. "2_06"
#       BASS_DLL   <path>          # absolute path to that version's bass.dll
#       SDK_TARGET <target_name>   # the STATIC/OBJECT lib with the SDK code
#   )
#
# The resulting target is bass_proxy_<VERSION>.  Its output file lives in
# CMAKE_CURRENT_BINARY_DIR (build/<version>/), so parallel versions never
# collide.  The deploy step in the 2_XX CMakeLists copies it to bass.dll.
function(add_bass_proxy)
    set(options "")
    set(one_value_args VERSION BASS_DLL SDK_TARGET)
    set(multi_value_args "")
    cmake_parse_arguments(PARSE_ARGV 0 ARG "${options}" "${one_value_args}"
                          "${multi_value_args}")

    if(NOT ARG_VERSION OR NOT ARG_BASS_DLL OR NOT ARG_SDK_TARGET)
        message(
            FATAL_ERROR
                "add_bass_proxy: VERSION, BASS_DLL, SDK_TARGET are required")
    endif()

    if(NOT TARGET ${ARG_SDK_TARGET})
        message(
            FATAL_ERROR
                "add_bass_proxy: SDK target '${ARG_SDK_TARGET}' not found. "
                "Ensure src/proxy/CMakeLists.txt is processed first."
        )
    endif()

    generate_proxy_def("${ARG_BASS_DLL}" def_file)

    # ── Version stub — exposes version string to the SDK at runtime ───────
    # The SDK static lib is compiled once (version-agnostic).  This tiny
    # source is compiled per proxy target so BASS_VERSION is visible.

    set(version_stub "${CMAKE_CURRENT_BINARY_DIR}/version_stub_${ARG_VERSION}.cpp")
    file(
        WRITE "${version_stub}"
        "// Auto-generated — exposes version to the SDK at runtime.\n"
        "namespace sdk { const char* const k_version = \"${ARG_VERSION}\"; }\n"
    )

    set(target_name "bass_proxy_${ARG_VERSION}")

    add_library(${target_name} SHARED "${def_file}" "${version_stub}")

    target_link_libraries(${target_name} PRIVATE ${ARG_SDK_TARGET})

    set_target_properties(
        ${target_name}
        PROPERTIES CXX_EXTENSIONS OFF
                   PREFIX ""
                   # No OUTPUT_NAME — the target's natural name is fine.
                   # Deploy step copies to bass.dll.
    )

    target_compile_definitions(${target_name}
                               PRIVATE "BASS_VERSION=${ARG_VERSION}")

    target_link_options(
        ${target_name}
        PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-static>
        $<$<CXX_COMPILER_ID:GNU,Clang>:-s>)

    message(
        STATUS
            "proxy target '${target_name}' — bass: ${ARG_BASS_DLL}"
    )
endfunction()
