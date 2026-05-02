function(generate_proxy_def input_dll def_out_var msvc_out_var)
    find_program(
        objdump_bin
        NAMES llvm-objdump objdump
        HINTS
            "${CMAKE_SOURCE_DIR}/llvm_mingw/bin"
            "${CMAKE_SOURCE_DIR}/llvm-mingw/bin"
        NO_DEFAULT_PATH)

    if(NOT objdump_bin)
        find_program(objdump_bin NAMES llvm-objdump objdump REQUIRED)
    endif()

    cmake_path(GET input_dll STEM LAST_ONLY dll_stem)
    set(def_file "${CMAKE_CURRENT_BINARY_DIR}/${dll_stem}.def")
    set(msvc_file "${CMAKE_CURRENT_BINARY_DIR}/${dll_stem}_msvc_exports.cpp")

    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${input_dll}")

    message(STATUS "parsing exports '${objdump_bin}': ${input_dll}")

    execute_process(
        COMMAND "${objdump_bin}" -p "${input_dll}"
        OUTPUT_VARIABLE dump_output
        OUTPUT_STRIP_TRAILING_WHITESPACE
        COMMAND_ERROR_IS_FATAL ANY
        COMMAND_ECHO STDOUT)

    string(REGEX MATCHALL "[0-9]+[ \\t]+0x[0-9a-fA-F]+[ \\t]+[^ \\t\\r\\n]+" raw_exports
           "${dump_output}")

    list(LENGTH raw_exports export_count)
    if(export_count EQUAL 0)
        message(FATAL_ERROR "generate_proxy_def: no exports in '${input_dll}'")
    endif()

    # .def for GNU ld / lld (MinGW, llvm-mingw, lld-link)
    set(def_content "LIBRARY \"${dll_stem}.dll\"\\nEXPORTS\\n")
    # MSVC pragma file — ordinals must be explicit so the game loader finds
    # the right entries (it imports BASS by ordinal).
    set(msvc_content "// Auto-generated proxy exports for MSVC. Do not edit.\\n")

    foreach(row IN LISTS raw_exports)
        string(REGEX REPLACE "([0-9]+)[ \\t]+0x[0-9a-fA-F]+[ \\t]+([^ \\t\\r\\n]+)"
               "\\\\2" sym_name "${row}")
        string(REGEX REPLACE "([0-9]+)[ \\t]+0x[0-9a-fA-F]+[ \\t]+([^ \\t\\r\\n]+)"
               "\\\\1" ordinal "${row}")
        string(APPEND def_content "    ${sym_name}=original.${sym_name} @${ordinal}\\n")
        string(APPEND msvc_content "#pragma comment(linker, \\\"/export:${sym_name}=original.${sym_name},@${ordinal}\\\")\\n")
    endforeach()

    file(WRITE "${def_file}" "${def_content}")
    file(WRITE "${msvc_file}" "${msvc_content}")
    message(STATUS "generated '${def_file}' and '${msvc_file}' (${export_count} exports)")

    set(${def_out_var} "${def_file}")
    set(${msvc_out_var} "${msvc_file}")
    return(PROPAGATE ${def_out_var} ${msvc_out_var})
endfunction()

generate_proxy_def("${CMAKE_SOURCE_DIR}/2_06/bass.dll" bass_proxy_def bass_proxy_msvc_cpp)

file(GLOB_RECURSE sdk_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/*.cpp")

# MSVC (cl.exe) cannot parse .def forwarders (LNK2001), so it uses pragma directives.
# Clang, MinGW, and every other linker (lld-link, GNU ld) handle the .def file fine.
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    add_library(bass_proxy SHARED ${bass_proxy_msvc_cpp} ${sdk_sources})
else()
    add_library(bass_proxy SHARED ${bass_proxy_def} ${sdk_sources})
endif()

target_include_directories(bass_proxy PRIVATE "${CMAKE_CURRENT_LIST_DIR}")
target_compile_features(bass_proxy PRIVATE cxx_std_23)
set_target_properties(bass_proxy PROPERTIES CXX_EXTENSIONS OFF PREFIX "" OUTPUT_NAME "bass")

target_compile_definitions(
    bass_proxy
    PRIVATE WIN32_LEAN_AND_MEAN
            NOMINMAX
            GLM_FORCE_RADIANS
            GLM_FORCE_DEPTH_ZERO_TO_ONE)

# ── link opts ────────────────────────────────────────────────────────────────
# v1.0.5 had -static -s unconditionally.  MSVC link.exe ignores -s (unknown
# option), while MinGW/Clang use it to strip symbols and static-link the CRT.
# Keeping the unconditional form preserves the old proven behaviour.
target_link_options(
    bass_proxy
    PRIVATE
    -static
    -s)

# Size optimisations: the proxy grew from ~1.7 MB (v1.0.5 single-file proxy)
# to ~3.7 MB because of the modular SDK (lua/sol2 scripting, spdlog,
# imgui+freetype backends, zydis/zycore via safetyhook).
# /OPT:REF,ICF (MSVC) and --gc-sections (GNU/Clang) remove unreachable code.
if(MSVC)
    target_link_options(bass_proxy PRIVATE /OPT:REF /OPT:ICF)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(
        bass_proxy
        PRIVATE
        -ffunction-sections
        -fdata-sections)
    target_link_options(bass_proxy PRIVATE -Wl,--gc-sections)
endif()

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
