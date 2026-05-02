#include "sdk/core/logging.hpp"
#include "sdk/sdk.hpp"

#include <spdlog/spdlog.h>
#include <thread>
#include <windows.h>

// bass_proxy.hpp presumably handles the actual DLL proxy forwarding.
// DllMain just calls our SDK install/uninstall.

BOOL APIENTRY DllMain(HMODULE  h_module,
                      DWORD    reason,
                      LPVOID   lp_reserved)
{
    switch (reason)
    {
        case DLL_PROCESS_ATTACH:
        {
            DisableThreadLibraryCalls(h_module);
            sdk::logging::init("logs");

            spdlog::set_level(spdlog::level::info);
            spdlog::flush_on(spdlog::level::info);
            spdlog::info("[bass_proxy] DLL_PROCESS_ATTACH pid={}", GetCurrentProcessId());
            spdlog::info("[bass_proxy] h_module={:p}", reinterpret_cast<void*>(h_module));
            spdlog::default_logger()->flush();

            // Install hooks on a separate thread to avoid loader lock issues.
            std::jthread([]() static {
                spdlog::info("[bass_proxy] hook installer thread started");
                spdlog::default_logger()->flush();
                try
                {
                    sdk::install_hooks();
                    spdlog::info("[bass_proxy] install_hooks completed OK");
                }
                catch (const std::exception& e)
                {
                    spdlog::error("[bass_proxy] install_hooks threw: {}", e.what());
                }
                catch (...)
                {
                    spdlog::error("[bass_proxy] install_hooks threw: unknown exception");
                }
                spdlog::default_logger()->flush();
            }).detach();
            break;
        }

        case DLL_PROCESS_DETACH:
        {
            spdlog::info("[bass_proxy] DLL_PROCESS_DETACH");
            spdlog::default_logger()->flush();

            // lp_reserved == nullptr  → FreeLibrary() call (dynamic unload).
            // lp_reserved != nullptr  → process is terminating. In that state
            // other threads are dead and the heap may be gone; calling
            // uninstall_hooks() is undefined behaviour.
            if (lp_reserved == nullptr)
            {
                try
                {
                    sdk::uninstall_hooks();
                    spdlog::info("[bass_proxy] uninstall_hooks completed OK");
                }
                catch (...)
                {
                    spdlog::error("[bass_proxy] uninstall_hooks threw");
                }
            }
            else
            {
                spdlog::info("[bass_proxy] process terminating — skipping uninstall");
            }

            spdlog::info("[bass_proxy] detached");
            spdlog::default_logger()->flush();

            sdk::logging::shutdown();
            break;
        }

        default:
            break;
    }

    return TRUE;
}
