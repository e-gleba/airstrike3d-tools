/// @file platform.hpp
/// @brief Input and window helpers (standard-library types only).

#pragma once

#include "sdk/core/types.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace sdk::platform
{

[[nodiscard]] bool is_key_down(std::int32_t vk) noexcept;
[[nodiscard]] vec2 get_cursor_pos() noexcept;
void set_cursor_pos(std::int32_t x, std::int32_t y) noexcept;
void show_cursor(bool visible) noexcept;
[[nodiscard]] rect get_window_rect() noexcept;

void log_info(std::string_view message);
void log_warn(std::string_view message);
void log_error(std::string_view message);
[[nodiscard]] constexpr std::string_view get_log_dir() noexcept { return "logs"; }
void send_chars(std::string_view chars) noexcept;

} // namespace sdk::platform
