cpmaddpackage(
    NAME tracy
    VERSION 0.13.0
    GITHUB_REPOSITORY wolfpld/tracy
    SYSTEM ON
    GIT_SHALLOW ON
    OPTIONS
        # build config
        "TRACY_STATIC ON"
        "TRACY_LTO OFF"
        
        # core functionality
        "TRACY_ENABLE ON"                      # master switch
        "TRACY_ON_DEMAND OFF"                  # start profiling only when connected to server
        "TRACY_DELAYED_INIT OFF"               # defer init until first tracy call
        "TRACY_MANUAL_LIFETIME OFF"            # manual profile start/stop (requires DELAYED_INIT=ON)
        
        # callstack options
        "TRACY_CALLSTACK OFF"                  # force callstack capture for all zones (perf hit)
        "TRACY_NO_CALLSTACK OFF"               # disable callstack entirely
        "TRACY_NO_CALLSTACK_INLINES OFF"       # strip inline funcs from stacks
        "TRACY_LIBUNWIND_BACKTRACE OFF"        # use libunwind instead of platform default
        "TRACY_SYMBOL_OFFLINE_RESOLVE OFF"     # defer symbol resolution to offline analysis
        "TRACY_LIBBACKTRACE_ELF_DYNLOAD_SUPPORT OFF"  # support for dynamically loaded libs
        
        # network/discovery
        "TRACY_ONLY_LOCALHOST OFF"             # bind to 127.0.0.1 only
        "TRACY_NO_BROADCAST OFF"               # disable LAN discovery broadcasts
        "TRACY_ONLY_IPV4 OFF"                  # ipv4-only mode
        
        # data capture toggles
        "TRACY_NO_CODE_TRANSFER OFF"           # disable source code fetch
        "TRACY_NO_CONTEXT_SWITCH OFF"          # disable OS context switch events
        "TRACY_NO_SAMPLING OFF"                # disable statistical sampling
        "TRACY_NO_VSYNC_CAPTURE OFF"           # disable vsync event capture
        "TRACY_NO_FRAME_IMAGE OFF"             # disable frame screenshots
        "TRACY_NO_SYSTEM_TRACING OFF"          # disable systrace (linux ftrace/android)
        "TRACY_NO_EXIT OFF"                    # block app exit until profile uploaded
        
        # validation/debugging
        "TRACY_NO_VERIFY OFF"                  # skip zone validation in C api
        "TRACY_NO_CRASH_HANDLER OFF"           # disable crash handler
        "TRACY_IGNORE_MEMORY_FAULTS OFF"       # ignore unmatched free() events
        "TRACY_VERBOSE OFF"                    # spam console with profiler internals
        
        # platform workarounds
        "TRACY_TIMER_FALLBACK OFF"             # use lower-res timers (ancient hw)
        "TRACY_PATCHABLE_NOPSLEDS OFF"         # nop sleds for rr/gdb patching
        "TRACY_DEMANGLE OFF"                   # disable default symbol demangling
        
        # optional integrations
        "TRACY_FIBERS OFF"                     # fiber/coroutine support
        "TRACY_DEBUGINFOD OFF"                 # debuginfod symbol server (needs libdebuginfod)
        "TRACY_Fortran OFF"                    # fortran bindings
        "TRACY_CLIENT_PYTHON OFF"              # python bindings (forces shared lib)
        
        # rocm gpu profiling (auto-detected if /opt/rocm exists)
        # "TRACY_ROCPROF_CALIBRATION OFF"      # continuous gpu time calibration (rocm only)
)

# usage:
# target_link_libraries(your_target PRIVATE Tracy::TracyClient)
#
# wrap zones like:
#   ZoneScoped;  // or ZoneScopedN("custom_name")
#   // your code
#
# memory tracking:
#   TracyAlloc(ptr, size);
#   TracyFree(ptr);
#
# gpu (vulkan example):
#   TracyVkContext(ctx, device, queue, cmdbuf);
#   TracyVkZone(ctx, cmdbuf, "gpu_work");
#
# connect profiler gui to see flamegraphs. server: https://github.com/wolfpld/tracy/releases
