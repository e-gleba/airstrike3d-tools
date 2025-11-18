#include "bass_proxy.hpp"

#include <thread>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

void safe_install() noexcept
{
    try
    {
        install_hooks();
    }
    catch (...)
    {
        // In a proxy DLL, we usually can't log easily, but we must
        // prevent the exception from crashing the host process.
    }
}

extern "C" BOOL APIENTRY DllMain(HMODULE h_module,
                                 DWORD   reason,
                                 LPVOID  reserved)
{
    switch (reason)
    {
        case DLL_PROCESS_ATTACH:
        {
            // Optimization: Prevent DllMain calls for thread
            // creation/destruction in this DLL. This reduces overhead and
            // loader lock contention.
            ::DisableThreadLibraryCalls(h_module);

            // DANGER: Spawning a std::thread inside DllMain invokes the Windows
            // Loader Lock. However, std::thread implementations often try to
            // mitigate this. If strict "Correctness" is required, one should
            // use CreateThread via WinAPI directly to minimize CRT
            // initialization risks, but you requested C++26.

            // We detach immediately to prevent blocking the loader.
            std::thread(safe_install).detach();
            break;
        }

        case DLL_PROCESS_DETACH:
        {
            // CRITICAL FIX: Check 'reserved'.
            // If reserved is NOT nullptr, the process is terminating (crashing
            // or exit() called). In this state, the heap might be gone, and
            // other threads are killed. Calling remove_hooks() here is
            // dangerous/undefined. Only clean up if the DLL is being unloaded
            // dynamically (FreeLibrary).
            if (reserved == nullptr)
            {
                uninstall_hooks();
            }
            break;
        }

        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
            // Explicitly ignored via DisableThreadLibraryCalls,
            // but standard switch practice suggests handling or ignoring them.
            break;
    }

    return TRUE;
}