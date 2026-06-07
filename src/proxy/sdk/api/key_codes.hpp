#pragma once

// sdk/api/key_codes.hpp — Virtual key codes (platform-independent constants)

#include <cstdint>

namespace sdk::api
{

// Virtual key codes (subset, matches WinAPI values)
inline constexpr int k_vk_lbutton   = 0x01;
inline constexpr int k_vk_rbutton   = 0x02;
inline constexpr int k_vk_mbutton   = 0x04;
inline constexpr int k_vk_xbutton1  = 0x05;
inline constexpr int k_vk_xbutton2  = 0x06;
inline constexpr int k_vk_back      = 0x08;
inline constexpr int k_vk_tab       = 0x09;
inline constexpr int k_vk_return    = 0x0D;
inline constexpr int k_vk_shift     = 0x10;
inline constexpr int k_vk_control   = 0x11;
inline constexpr int k_vk_menu      = 0x12;
inline constexpr int k_vk_escape    = 0x1B;
inline constexpr int k_vk_space     = 0x20;
inline constexpr int k_vk_prior     = 0x21;
inline constexpr int k_vk_next      = 0x22;
inline constexpr int k_vk_end       = 0x23;
inline constexpr int k_vk_home      = 0x24;
inline constexpr int k_vk_left      = 0x25;
inline constexpr int k_vk_up        = 0x26;
inline constexpr int k_vk_right     = 0x27;
inline constexpr int k_vk_down      = 0x28;
inline constexpr int k_vk_insert    = 0x2D;
inline constexpr int k_vk_delete    = 0x2E;
inline constexpr int k_vk_capital   = 0x14;

// F1-F24
inline constexpr int k_vk_f1  = 0x70;
inline constexpr int k_vk_f2  = 0x71;
inline constexpr int k_vk_f3  = 0x72;
inline constexpr int k_vk_f4  = 0x73;
inline constexpr int k_vk_f5  = 0x74;
inline constexpr int k_vk_f6  = 0x75;
inline constexpr int k_vk_f7  = 0x76;
inline constexpr int k_vk_f8  = 0x77;
inline constexpr int k_vk_f9  = 0x78;
inline constexpr int k_vk_f10 = 0x79;
inline constexpr int k_vk_f11 = 0x7A;
inline constexpr int k_vk_f12 = 0x7B;

} // namespace sdk::api
