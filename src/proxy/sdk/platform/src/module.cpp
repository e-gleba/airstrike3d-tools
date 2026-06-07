// sdk/platform/module.cpp — WinAPI implementation of module utilities

#include "sdk/platform/module.hpp"

#include <windows.h>

namespace sdk::platform
{

auto get_module(std::wstring_view name) noexcept -> module_handle*
{
    auto h = name.empty() ? GetModuleHandleW(nullptr)
                          : GetModuleHandleW(std::wstring(name).c_str());
    return reinterpret_cast<module_handle*>(h);
}

auto get_proc_address(module_handle* mod, std::string_view proc_name) noexcept -> void*
{
    if (!mod || proc_name.empty())
    {
        return nullptr;
    }
    // Ensure null-terminated
    auto name_str = std::string(proc_name);
    return reinterpret_cast<void*>(
        GetProcAddress(reinterpret_cast<HMODULE>(mod), name_str.c_str()));
}

auto is_module_loaded(std::wstring_view name) noexcept -> bool
{
    return GetModuleHandleW(std::wstring(name).c_str()) != nullptr;
}

} // namespace sdk::platform
