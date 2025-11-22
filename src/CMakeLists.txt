cmake_minimum_required(VERSION 3.20)

function(generate_proxy_def input_dll output_var)
    find_program(LLVM_OBJDUMP llvm-objdump REQUIRED)

    # Get clean filename for the LIBRARY directive (e.g., "BASS" from "bass.dll")
    cmake_path(GET input_dll STEM dll_stem)
    set(def_file "${CMAKE_CURRENT_BINARY_DIR}/${dll_stem}.def")

    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${input_dll}")

    message(STATUS "Parsing exports for proxy: ${input_dll}")
    execute_process(
        COMMAND ${LLVM_OBJDUMP} -p ${input_dll}
        OUTPUT_VARIABLE dump_output
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE result
    )

    if(NOT result EQUAL 0)
        message(FATAL_ERROR "llvm-objdump failed on ${input_dll}")
    endif()

    # Parse exports (Ordinal + RVA + Name)
    string(REGEX MATCHALL
           "[0-9]+[ \t]+0x[0-9a-fA-F]+[ \t]+[^ \t\r\n]+"
           raw_exports "${dump_output}")

    set(content "LIBRARY \"${dll_stem}.dll\"\nEXPORTS\n")

    foreach(row IN LISTS raw_exports)
        # Extract Name (group 2) and Ordinal (group 1)
        string(REGEX REPLACE "([0-9]+)[ \t]+0x[0-9a-fA-F]+[ \t]+([^ \t\r\n]+)" "\\2" name "${row}")
        string(REGEX REPLACE "([0-9]+)[ \t]+0x[0-9a-fA-F]+[ \t]+([^ \t\r\n]+)" "\\1" ordinal "${row}")

        # Forward to 'original.dll'
        string(APPEND content "    ${name}=original.${name} @${ordinal}\n")
    endforeach()

    file(WRITE ${def_file} "${content}")

    set(${output_var} "${def_file}" PARENT_SCOPE)
endfunction()

generate_proxy_def(
    ${CMAKE_SOURCE_DIR}/2.06/bass.dll
    bass_proxy_definitions
)

add_library(bass_proxy SHARED bass_proxy.cpp ${bass_proxy_definitions}
                                      dll_main.cpp)
target_include_directories(bass_proxy PRIVATE ${CMAKE_CURRENT_LIST_DIR})

set_target_properties(
    bass_proxy
    PROPERTIES     CXX_STANDARD 26
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

target_link_libraries(bass_proxy PRIVATE warnings imgui_opengl3 dwmapi glm minhook EnTT::EnTT)