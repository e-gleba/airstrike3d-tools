#include "sdk/core/logging.hpp"
#include "sdk/sdk.hpp"

#include <thread>
#include <windows.h>

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
            sdk::logging::set_level(sdk::logging::level::info);

            sdk::log_info("[bass_proxy] attached");

            std::jthread([]() static { sdk::install_hooks(); }).detach();
            break;
        }

        case DLL_PROCESS_DETACH:
        {
            // The proxy is pinned after hook installation. Process teardown
            // owns all remaining resources; doing hook, Lua, COM, or ImGui
            // cleanup while the Windows loader lock is held is unsafe.
            break;
        }

        default:
        {
            break;
        }
    }

    return TRUE;
}
