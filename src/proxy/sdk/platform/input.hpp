#pragma once

// sdk/platform/input.hpp — Input queries (pure C++23 interface)

#include "sdk/platform/types.hpp"

namespace sdk::platform
{

// Check if key is currently pressed
[[nodiscard]] auto is_key_down(int vk_code) noexcept -> bool;

// Get current cursor position
[[nodiscard]] auto get_cursor_pos() noexcept -> cursor_pos;

// Set cursor position
void set_cursor_pos(std::int32_t x, std::int32_t y) noexcept;

// Show/hide cursor
void show_cursor(bool visible) noexcept;

} // namespace sdk::platform
