add_executable(proxy_rendering_tests
               rendering_tests.cpp
               "${AS3D_PROJECT_ROOT}/src/proxy/sdk/graphics/rendering.cpp"
               "${AS3D_PROJECT_ROOT}/src/proxy/sdk/graphics/detail/camera_math.cpp")

target_include_directories(proxy_rendering_tests
                           PRIVATE "${AS3D_PROJECT_ROOT}/src/proxy")
target_link_libraries(proxy_rendering_tests PRIVATE as3d_build_options warnings)
target_link_options(
    proxy_rendering_tests
    PRIVATE $<$<CXX_COMPILER_ID:Clang>:-static>)

set_target_properties(
    proxy_rendering_tests
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
               RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
               RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
               CXX_EXTENSIONS OFF)

set(game_exe "proxy_rendering_tests.exe")
configure_file("${AS3D_PROJECT_ROOT}/cmake/run_with_proton.sh.in"
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
