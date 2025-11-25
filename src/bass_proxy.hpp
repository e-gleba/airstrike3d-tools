#pragma once

#ifdef _WIN32
#define PROXY_EXPORT extern "C" __declspec(dllexport)
#else
#define PROXY_EXPORT extern "C" __attribute__((visibility("default")))
#endif

PROXY_EXPORT void install_hooks();

PROXY_EXPORT void uninstall_hooks();