#include "sdk/sdk.hpp"
#include "sdk/core/hooks.hpp"
#include "sdk/core/logging.hpp"

#include <spdlog/spdlog.h>
#include <thread>
#include <windows.h>

BOOL APIENTRY DllMain(HMODULE h_module, DWORD reason,
                      [[maybe_unused]] LPVOID lp_reserved) {
    switch (reason) {
    case DLL_PROCESS_ATTACH: {
        DisableThreadLibraryCalls(h_module);

        sdk::logging::init("logs");
        spdlog::set_level(spdlog::level::info);
        spdlog::info("[bass_proxy] attached");

        std::jthread([]() static { sdk::install_hooks(); }).detach();
        break;
    }

    case DLL_PROCESS_DETACH: {
        sdk::uninstall_hooks();
        spdlog::info("[bass_proxy] detached");

        sdk::logging::shutdown();
        break;
    }

    default: {
        break;
    }
    }

    return TRUE;
}
