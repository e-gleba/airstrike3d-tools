add_executable(proxy_rendering_tests
               rendering_tests.cpp
               "${CMAKE_SOURCE_DIR}/src/proxy/sdk/graphics/rendering.cpp")

target_include_directories(proxy_rendering_tests
                           PRIVATE "${CMAKE_SOURCE_DIR}/src/proxy")
target_compile_features(proxy_rendering_tests PRIVATE cxx_std_23)
target_link_libraries(proxy_rendering_tests PRIVATE warnings)
target_link_options(
    proxy_rendering_tests
    PRIVATE $<$<CXX_COMPILER_ID:Clang>:-static>)

set_target_properties(
    proxy_rendering_tests
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
               RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
               RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}")

set(game_exe "proxy_rendering_tests.exe")
configure_file("${CMAKE_SOURCE_DIR}/cmake/run_with_proton.sh.in"
               "${CMAKE_CURRENT_BINARY_DIR}/run_tests.sh"
               @ONLY
               FILE_PERMISSIONS
               OWNER_READ OWNER_WRITE OWNER_EXECUTE
               GROUP_READ GROUP_EXECUTE
               WORLD_READ WORLD_EXECUTE)

add_test(NAME proxy_rendering_state
         COMMAND "${CMAKE_CURRENT_BINARY_DIR}/run_tests.sh")
set_tests_properties(proxy_rendering_state
                     PROPERTIES LABELS "proxy;unit"
                                TIMEOUT 30)
