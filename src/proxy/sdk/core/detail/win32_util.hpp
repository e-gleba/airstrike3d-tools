/// @file win32_util.hpp
/// @brief Win32 helpers for hook installation (internal only).

#pragma once

#include <windows.h>

namespace sdk::detail
{

[[nodiscard]] inline void* proc_addr(const wchar_t* mod, const char* fn) noexcept
{
    return reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(mod), fn));
}

} // namespace sdk::detail
