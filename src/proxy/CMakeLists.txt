function(generate_proxy_def input_dll output_var)
    find_program(objdump_bin NAMES llvm-objdump objdump
                 HINTS "${CMAKE_SOURCE_DIR}/llvm-mingw/bin" REQUIRED)

    cmake_path(
        GET
        input_dll
        STEM
        dll_stem)
    set(def_file "${CMAKE_CURRENT_BINARY_DIR}/${dll_stem}.def")

    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
                                           "${input_dll}")

    message(STATUS "parsing exports '${objdump_bin}': ${input_dll}")

    execute_process(
        COMMAND ${objdump_bin} -p "${input_dll}" OUTPUT_VARIABLE dump_output
        OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)

    string(
        REGEX MATCHALL
              "[0-9]+[ \t]+0x[0-9a-fA-F]+[ \t]+[^ \t\r\n]+"
              raw_exports
              "${dump_output}")

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

    set(${output_var} "${def_file}")
    return(PROPAGATE ${output_var})
endfunction()

generate_proxy_def("${CMAKE_SOURCE_DIR}/2_06/bass.dll" bass_proxy_definitions)

# TODO: as we have some aspect sensitivity problems, so its impossible to use
# cmaera now with the all ui, so to compile camera, replace bass_proxy.cpp here
# with todo_camera.cpp
add_library(bass_proxy SHARED ${bass_proxy_definitions} todo_camera.cpp
                              dll_main.cpp)
target_include_directories(bass_proxy PRIVATE ${CMAKE_CURRENT_LIST_DIR})

set_target_properties(
    bass_proxy
    PROPERTIES CXX_STANDARD 26
               CXX_STANDARD_REQUIRED ON
               CXX_EXTENSIONS OFF
               POSITION_INDEPENDENT_CODE ON
               PREFIX "" # no "lib" prefix
    )

target_link_options(
    bass_proxy
    PRIVATE
    -static
    -s)

target_link_libraries(
    bass_proxy
    PRIVATE warnings
            imgui_opengl3
            dwmapi
            glm
            minhook
            EnTT::EnTT
            safetyhook
            spdlog::spdlog)