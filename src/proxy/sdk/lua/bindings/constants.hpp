/// @file bindings/constants.hpp
/// @brief Pure C++ constant accessors for Lua binding (no sol2 types).

#pragma once

#include <cstdint>

namespace sdk::lua::bindings::constants
{

// ── Virtual keys ─────────────────────────────────────────────────────────────

// Modifiers
[[nodiscard]] std::int32_t vk_shift() noexcept;
[[nodiscard]] std::int32_t vk_control() noexcept;
[[nodiscard]] std::int32_t vk_space() noexcept;

// Navigation
[[nodiscard]] std::int32_t vk_insert() noexcept;
[[nodiscard]] std::int32_t vk_escape() noexcept;
[[nodiscard]] std::int32_t vk_tab() noexcept;
[[nodiscard]] std::int32_t vk_return() noexcept;
[[nodiscard]] std::int32_t vk_back() noexcept;
[[nodiscard]] std::int32_t vk_delete() noexcept;
[[nodiscard]] std::int32_t vk_home() noexcept;
[[nodiscard]] std::int32_t vk_end() noexcept;
[[nodiscard]] std::int32_t vk_prior() noexcept;
[[nodiscard]] std::int32_t vk_next() noexcept;

// Arrow keys
[[nodiscard]] std::int32_t vk_left() noexcept;
[[nodiscard]] std::int32_t vk_right() noexcept;
[[nodiscard]] std::int32_t vk_up() noexcept;
[[nodiscard]] std::int32_t vk_down() noexcept;

// F-keys
[[nodiscard]] std::int32_t vk_f1() noexcept;
[[nodiscard]] std::int32_t vk_f2() noexcept;
[[nodiscard]] std::int32_t vk_f3() noexcept;
[[nodiscard]] std::int32_t vk_f4() noexcept;
[[nodiscard]] std::int32_t vk_f5() noexcept;
[[nodiscard]] std::int32_t vk_f6() noexcept;
[[nodiscard]] std::int32_t vk_f7() noexcept;
[[nodiscard]] std::int32_t vk_f8() noexcept;
[[nodiscard]] std::int32_t vk_f9() noexcept;
[[nodiscard]] std::int32_t vk_f10() noexcept;
[[nodiscard]] std::int32_t vk_f11() noexcept;
[[nodiscard]] std::int32_t vk_f12() noexcept;

// Mouse buttons
[[nodiscard]] std::int32_t vk_lbutton() noexcept;
[[nodiscard]] std::int32_t vk_rbutton() noexcept;
[[nodiscard]] std::int32_t vk_mbutton() noexcept;

// Letter keys (movement / hotkeys)
[[nodiscard]] std::int32_t vk_w() noexcept;
[[nodiscard]] std::int32_t vk_a() noexcept;
[[nodiscard]] std::int32_t vk_s() noexcept;
[[nodiscard]] std::int32_t vk_d() noexcept;
[[nodiscard]] std::int32_t vk_q() noexcept;
[[nodiscard]] std::int32_t vk_e() noexcept;
[[nodiscard]] std::int32_t vk_c() noexcept;
[[nodiscard]] std::int32_t vk_r() noexcept;
[[nodiscard]] std::int32_t vk_z() noexcept;
[[nodiscard]] std::int32_t vk_x() noexcept;
[[nodiscard]] std::int32_t vk_v() noexcept;

// ── OpenGL constants ─────────────────────────────────────────────────────────

// Matrix mode
[[nodiscard]] std::int32_t gl_modelview() noexcept;
[[nodiscard]] std::int32_t gl_projection() noexcept;
[[nodiscard]] std::int32_t gl_texture() noexcept;

// State caps
[[nodiscard]] std::int32_t gl_depth_test() noexcept;
[[nodiscard]] std::int32_t gl_blend() noexcept;
[[nodiscard]] std::int32_t gl_alpha_test() noexcept;
[[nodiscard]] std::int32_t gl_cull_face() noexcept;
[[nodiscard]] std::int32_t gl_lighting() noexcept;
[[nodiscard]] std::int32_t gl_fog() noexcept;
[[nodiscard]] std::int32_t gl_texture_2d() noexcept;

// Face selection
[[nodiscard]] std::int32_t gl_front() noexcept;
[[nodiscard]] std::int32_t gl_back() noexcept;
[[nodiscard]] std::int32_t gl_front_and_back() noexcept;

// Blend factors
[[nodiscard]] std::int32_t gl_src_alpha() noexcept;
[[nodiscard]] std::int32_t gl_one_minus_src_alpha() noexcept;
[[nodiscard]] std::int32_t gl_one() noexcept;
[[nodiscard]] std::int32_t gl_zero() noexcept;

// Primitive types (for glBegin)
[[nodiscard]] std::int32_t gl_lines() noexcept;
[[nodiscard]] std::int32_t gl_line_strip() noexcept;
[[nodiscard]] std::int32_t gl_line_loop() noexcept;
[[nodiscard]] std::int32_t gl_triangles() noexcept;
[[nodiscard]] std::int32_t gl_triangle_strip() noexcept;
[[nodiscard]] std::int32_t gl_triangle_fan() noexcept;
[[nodiscard]] std::int32_t gl_quads() noexcept;
[[nodiscard]] std::int32_t gl_points() noexcept;
[[nodiscard]] std::int32_t gl_polygon() noexcept;

// Polygon mode (for glPolygonMode)
[[nodiscard]] std::int32_t gl_line() noexcept;
[[nodiscard]] std::int32_t gl_fill() noexcept;

// State masks (for glPushAttrib / glPopAttrib)
[[nodiscard]] std::int32_t gl_all_attrib_bits() noexcept;

} // namespace sdk::lua::bindings::constants
