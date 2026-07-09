# proxy_utils.cmake — DLL proxy generation helpers (requires CMake 3.25+)
#
# Provides:
#   generate_proxy_def(<input_dll> <output_var>)
#   add_bass_proxy(VERSION <v> BASS_DLL <path> SDK_TARGET <tgt>)

include_guard(GLOBAL)

find_program(
    OBJDUMP_EXECUTABLE
    NAMES llvm-objdump objdump
    HINTS "${AS3D_PROJECT_ROOT}/llvm_mingw/bin" REQUIRED
    DOC "PE object dump utility for export parsing")

function(generate_proxy_def input_dll output_var)
    cmake_path(
        GET
        input_dll
        STEM
        LAST_ONLY
        dll_stem)
    set(def_file "${CMAKE_CURRENT_BINARY_DIR}/${dll_stem}.def")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
                                           "${input_dll}")

    message(STATUS "Parsing exports: ${input_dll}")
    execute_process(
        COMMAND "${OBJDUMP_EXECUTABLE}" -p "${input_dll}"
        OUTPUT_VARIABLE dump_output OUTPUT_STRIP_TRAILING_WHITESPACE
                                    COMMAND_ERROR_IS_FATAL ANY)

    string(
        REGEX MATCHALL
              "[0-9]+[ \t]+0x[0-9a-fA-F]+[ \t]+[^ \t\r\n]+"
              raw_exports
              "${dump_output}")
    if(NOT raw_exports)
        message(FATAL_ERROR "No exports found in '${input_dll}'")
    endif()

    list(TRANSFORM raw_exports
         REPLACE "([0-9]+)[ \t]+0x[0-9a-fA-F]+[ \t]+([^ \t\r\n]+)"
                 "    \\2=original.\\2 @\\1")
    list(
        JOIN
        raw_exports
        "\n"
        exports_block)

    file(WRITE "${def_file}"
         "LIBRARY \"${dll_stem}.dll\"\nEXPORTS\n${exports_block}\n")

    list(LENGTH raw_exports export_count)
    message(STATUS "Generated '${def_file}' (${export_count} exports)")

    set(${output_var} "${def_file}")
    return(PROPAGATE ${output_var})
endfunction()

function(add_bass_proxy)
    cmake_parse_arguments(
        PARSE_ARGV
        0
        arg
        ""
        "VERSION;BASS_DLL;SDK_TARGET"
        "")

    foreach(required IN ITEMS VERSION BASS_DLL SDK_TARGET)
        if(NOT arg_${required})
            message(FATAL_ERROR "add_bass_proxy: ${required} is required")
        endif()
    endforeach()

    if(NOT TARGET "${arg_SDK_TARGET}")
        message(FATAL_ERROR "SDK target '${arg_SDK_TARGET}' not found")
    endif()

    generate_proxy_def("${arg_BASS_DLL}" def_file)
    set(proxy_target "bass_proxy_${arg_VERSION}")

    add_library(${proxy_target} SHARED "${def_file}")
    target_link_libraries(${proxy_target} PRIVATE "${arg_SDK_TARGET}")

    set_target_properties(
        ${proxy_target}
        PROPERTIES PREFIX "" CXX_EXTENSIONS OFF
                   MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

    target_link_options(${proxy_target} PRIVATE
                        $<$<NOT:$<CXX_COMPILER_FRONTEND_VARIANT:MSVC>>:-static>)

    if(CMAKE_CXX_COMPILER_TARGET MATCHES "windows-msvc")
        target_link_libraries(
            ${proxy_target}
            PRIVATE $<IF:$<CONFIG:Debug>,msvcrtd,msvcrt>.lib
                    $<IF:$<CONFIG:Debug>,vcruntimed,vcruntime>.lib
                    $<IF:$<CONFIG:Debug>,ucrtd,ucrt>.lib)
    endif()

    message(STATUS "Proxy target '${proxy_target}' -> ${arg_BASS_DLL}")
endfunction()