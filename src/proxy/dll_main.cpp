// dll_main.cpp — DLL entry point using RAII engine
//
// Replaces manual init/shutdown with automatic RAII lifecycle.

#include "sdk/sdk.hpp"

#include <windows.h>
#include <thread>

namespace
{

// Global engine instance (RAII-managed)
std::unique_ptr<sdk::engine> g_engine;

} // namespace

BOOL APIENTRY DllMain(HMODULE h_module, DWORD reason, LPVOID lp_reserved)
{
    switch (reason)
    {
        case DLL_PROCESS_ATTACH:
        {
            DisableThreadLibraryCalls(h_module);

            // Configure engine
            sdk::config cfg{
                .log_dir         = "logs",
                .plugin_dir      = "plugins",
                .log_level       = "info",
                .enable_overlay  = true,
                .enable_scripting = true,
            };

            // Create and initialize engine (RAII)
            g_engine = std::make_unique<sdk::engine>(std::move(cfg));
            g_engine->init();

            // Install hooks in background thread
            std::jthread([]() {
                g_engine->install_hooks();
                g_engine->load_plugins();
            }).detach();

            break;
        }

        case DLL_PROCESS_DETACH:
        {
            // Engine destructor handles cleanup automatically (RAII)
            g_engine.reset();
            break;
        }

        default:
        {
            break;
        }
    }

    return TRUE;
}
