# For more skills browse: https://www.skills.sh/
# For more mcps browse: https://mcpservers.org/
cmake_minimum_required(VERSION 4.3)

find_program(node_executable NAMES node nodejs REQUIRED)
find_program(npx_executable NAMES npx npx.cmd REQUIRED)
find_program(python_executable NAMES python3 python REQUIRED)
find_program(git_executable NAMES git REQUIRED)

execute_process(
    COMMAND
        ${node_executable} --version COMMAND_ECHO STDOUT COMMAND_ERROR_IS_FATAL
        ANY)
execute_process(
    COMMAND
        ${npx_executable} --version COMMAND_ECHO STDOUT COMMAND_ERROR_IS_FATAL
        ANY)
execute_process(
    COMMAND
        ${python_executable} --version COMMAND_ECHO STDOUT
        COMMAND_ERROR_IS_FATAL ANY)
execute_process(
    COMMAND
        ${git_executable} --version COMMAND_ECHO STDOUT COMMAND_ERROR_IS_FATAL
        ANY)

execute_process(
    COMMAND ${git_executable} rev-parse --show-toplevel
    OUTPUT_VARIABLE git_root
    OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND_ECHO
    STDOUT
    COMMAND_ERROR_IS_FATAL
    ANY)

if(WIN32)
    set(npx_invoke cmd /c ${npx_executable})
else()
    set(npx_invoke ${npx_executable})
endif()

function(install_agent_skills)
    cmake_parse_arguments(
        PARSE_ARGV
        0
        arg
        ""
        "URL"
        "SKILLS")
    if(NOT arg_URL)
        message(FATAL_ERROR "install_agent_skills: URL missing")
    endif()
    set(skill_flags "")
    foreach(s IN LISTS arg_SKILLS)
        list(
            APPEND
            skill_flags
            --skill
            ${s})
    endforeach()
    execute_process(
        COMMAND
            ${CMAKE_COMMAND} -E env npm_config_package_lock=false CI=true
            NO_COLOR=1 TERM=dumb PYTHONUTF8=1 PYTHONIOENCODING=utf-8
            ${npx_invoke} --yes skills add ${arg_URL} -a opencode -y
            ${skill_flags}
        WORKING_DIRECTORY "${git_root}"
        TIMEOUT 600
                COMMAND_ECHO
                STDOUT
                COMMAND_ERROR_IS_FATAL
                ANY)
endfunction()

function(install_agent_mcp)
    cmake_parse_arguments(
        PARSE_ARGV
        0
        arg
        ""
        "URL;NAME"
        "")
    if(NOT arg_URL)
        message(FATAL_ERROR "install_agent_mcp: URL missing")
    endif()
    set(name_flag "")
    if(arg_NAME)
        set(name_flag --name ${arg_NAME})
    endif()
    execute_process(
        COMMAND
            ${CMAKE_COMMAND} -E env npm_config_package_lock=false CI=true
            NO_COLOR=1 TERM=dumb PYTHONUTF8=1 PYTHONIOENCODING=utf-8
            ${npx_invoke} --yes add-mcp ${arg_URL} -a opencode -y ${name_flag}
        WORKING_DIRECTORY "${git_root}"
        TIMEOUT 600
                COMMAND_ECHO
                STDOUT
                COMMAND_ERROR_IS_FATAL
                ANY)
endfunction()

install_agent_skills(
    URL
    https://github.com/vercel-labs/skills
    SKILLS
    find-skills)
install_agent_skills(
    URL
    https://github.com/mattpocock/skills
    SKILLS
    code-review
    caveman
    diagnose
    qa
    to-spec
    research
    to-tickets
    resolving-merge-conflicts)
install_agent_skills(
    URL
    https://github.com/mohitmishra786/low-level-dev-skills
    SKILLS
    gcc
    clang
    llvm
    cross-gcc
    pgo
    cpp-modules
    cpp-templates
    cpp-coroutines
    cmake
    make
    ninja
    meson
    conan-vcpkg
    static-analysis
    build-acceleration
    bazel
    include-what-you-use
    linker-scripts
    gdb
    lldb
    core-dumps
    concurrency-debugging
    debug-optimized-builds
    dwarf-debug-format
    linux-perf
    valgrind
    flamegraphs
    strace-ltrace
    heaptrack
    intel-vtune-amd-uprof
    hardware-counters
    sanitizers
    fuzzing
    binary-hardening
    elf-inspection
    linkers-lto
    binutils
    dynamic-linking
    assembly-x86
    assembly-arm
    assembly-riscv
    interpreters
    simd-intrinsics
    memory-model
    cpu-cache-opt
    custom-allocators
    numa-programming
    compiler-frontend
    llvm-passes
    llvm-ir-and-passes
    compiler-optimizations-deep
    code-generation-and-backends
    mlir
    jit-compilation
    linux-kernel-architecture
    kernel-memory-management
    kernel-concurrency
    device-tree
    platform-device-model
    writing-char-drivers
    bus-drivers-i2c-spi
    kernel-debugging-advanced
    qemu-for-kernel-development
    kernel-internals
    device-drivers
    kernel-debugging
    kernel-testing
    os-dev-scratch
    linux-kernel-modules
    io-uring
    dpdk
    af-xdp
    ebpf
    cuda
    cuda-profiling
    cuda-debugging
    triton-lang
    hip-rocm
    gpu-memory-model
    openmp
    mpi
    rdma-verbs
    qemu-kvm
    hypervisor-internals
    containers-internals
    qemu-embedded-simulation
    resource-optimization-lowend
    cpu-pipelines-and-hazards
    memory-hierarchy-and-caches
    virtual-memory-paging-and-tlb
    abi-and-calling-conventions
    branch-prediction-and-speculation
    reverse-engineering
    kernel-security
    arm-sve
    riscv-privileged
    apple-silicon
    wasm-emscripten
    wasm-wasmtime)

install_agent_mcp(
    URL
    https://api.githubcopilot.com/mcp/
    NAME
    githubcopilot)
install_agent_mcp(
    URL
    https://xdocs.dev/mcp
    NAME
    xdocs)
install_agent_mcp(
    URL
    https://qt-docs-mcp.qt.io/mcp
    NAME
    qt-docs)
install_agent_mcp(
    URL
    https://learn.microsoft.com/api/mcp
    NAME
    ms-learn)
install_agent_mcp(
    URL
    https://godbolt.org/mcp
    NAME
    godbolt)
install_agent_mcp(
    URL
    https://developerknowledge.googleapis.com/mcp
    NAME
    google-dev)
