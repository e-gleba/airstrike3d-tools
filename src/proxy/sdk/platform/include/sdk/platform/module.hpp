#pragma once

// sdk/platform/module.hpp — DLL/module utilities (pure C++23 interface)

#include "sdk/platform/types.hpp"

#include <string_view>

namespace sdk::platform
{

// Get module handle by name (nullptr for current executable)
[[nodiscard]] auto get_module(std::wstring_view name = {}) noexcept -> module_handle*;

// Get procedure address from module
[[nodiscard]] auto get_proc_address(module_handle* mod, std::string_view proc_name) noexcept -> void*;

// Check if module is loaded
[[nodiscard]] auto is_module_loaded(std::wstring_view name) noexcept -> bool;

} // namespace sdk::platform
