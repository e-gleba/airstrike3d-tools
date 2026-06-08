#include "sdk/sdk.hpp"
#include "sdk/scripting_backend.hpp"
#include "sdk/core/hooks.hpp"
#include "sdk/core/logging.hpp"

#include <spdlog/spdlog.h>
#include <windows.h>

// Forward declaration from sol2_backend.cpp
namespace sdk {
std::unique_ptr<scripting_backend> create_sol2_backend();
}

BOOL APIENTRY DllMain(HMODULE h_module, DWORD reason,
                      [[maybe_unused]] LPVOID lp_reserved) {
    switch (reason) {
    case DLL_PROCESS_ATTACH: {
        DisableThreadLibraryCalls(h_module);

        // Initialize logging
        sdk::logging::init("logs");
        spdlog::set_level(spdlog::level::info);
        spdlog::info("[bass_proxy] attached");

        // Create and set scripting backend (sol2)
        sdk::set_scripting_backend(sdk::create_sol2_backend());

        // Install hooks in separate thread
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
