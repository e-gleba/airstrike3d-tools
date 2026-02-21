#include "sdk/core/logging.hpp"
#include "sdk/sdk.hpp"

#include <spdlog/spdlog.h>
#include <thread>
#include <windows.h>

// bass_proxy.hpp presumably handles the actual DLL proxy forwarding.
// DllMain just calls our SDK install/uninstall.

BOOL APIENTRY DllMain(HMODULE                 h_module,
                      DWORD                   reason,
                      [[maybe_unused]] LPVOID lp_reserved)
{
    switch (reason)
    {
        case DLL_PROCESS_ATTACH:
        {
            DisableThreadLibraryCalls(h_module);
            sdk::logging::init("logs");

            spdlog::set_level(spdlog::level::info);
            spdlog::info("[bass_proxy] attached");

            // Install hooks on a separate thread to avoid loader lock issues.
            // C++26: std::jthread is preferred, but we detach immediately
            // so the semantics match the original. Using a plain lambda
            // with static operator() (P1169, merged for C++23/26).
            std::jthread([]() static { sdk::install_hooks(); }).detach();
            break;
        }

        case DLL_PROCESS_DETACH:
        {
            sdk::uninstall_hooks();
            spdlog::info("[bass_proxy] detached");

            sdk::logging::shutdown();
            break;
        }

        default:
            break;
    }

    return TRUE;
}
