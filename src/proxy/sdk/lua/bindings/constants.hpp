/// @file bindings/constants.hpp
/// @brief Pure C++ constant accessors for Lua binding (no sol2 types).

#pragma once

namespace sdk::lua::bindings::constants
{

// ── Virtual keys ─────────────────────────────────────────────────────────────

// Modifiers
[[nodiscard]] int vk_shift() noexcept;
[[nodiscard]] int vk_control() noexcept;
[[nodiscard]] int vk_space() noexcept;

// Navigation
[[nodiscard]] int vk_insert() noexcept;
[[nodiscard]] int vk_escape() noexcept;
[[nodiscard]] int vk_tab() noexcept;
[[nodiscard]] int vk_return() noexcept;
[[nodiscard]] int vk_back() noexcept;
[[nodiscard]] int vk_delete() noexcept;
[[nodiscard]] int vk_home() noexcept;
[[nodiscard]] int vk_end() noexcept;
[[nodiscard]] int vk_prior() noexcept;
[[nodiscard]] int vk_next() noexcept;

// Arrow keys
[[nodiscard]] int vk_left() noexcept;
[[nodiscard]] int vk_right() noexcept;
[[nodiscard]] int vk_up() noexcept;
[[nodiscard]] int vk_down() noexcept;

// F-keys
[[nodiscard]] int vk_f1() noexcept;
[[nodiscard]] int vk_f2() noexcept;
[[nodiscard]] int vk_f3() noexcept;
[[nodiscard]] int vk_f4() noexcept;
[[nodiscard]] int vk_f5() noexcept;
[[nodiscard]] int vk_f6() noexcept;
[[nodiscard]] int vk_f7() noexcept;
[[nodiscard]] int vk_f8() noexcept;
[[nodiscard]] int vk_f9() noexcept;
[[nodiscard]] int vk_f10() noexcept;
[[nodiscard]] int vk_f11() noexcept;
[[nodiscard]] int vk_f12() noexcept;

// Mouse buttons
[[nodiscard]] int vk_lbutton() noexcept;
[[nodiscard]] int vk_rbutton() noexcept;
[[nodiscard]] int vk_mbutton() noexcept;

// Letter keys (movement / hotkeys)
[[nodiscard]] int vk_w() noexcept;
[[nodiscard]] int vk_a() noexcept;
[[nodiscard]] int vk_s() noexcept;
[[nodiscard]] int vk_d() noexcept;
[[nodiscard]] int vk_q() noexcept;
[[nodiscard]] int vk_e() noexcept;
[[nodiscard]] int vk_c() noexcept;
[[nodiscard]] int vk_r() noexcept;
[[nodiscard]] int vk_z() noexcept;
[[nodiscard]] int vk_x() noexcept;
[[nodiscard]] int vk_v() noexcept;

// ── OpenGL constants ─────────────────────────────────────────────────────────

// Matrix mode
[[nodiscard]] int gl_modelview() noexcept;
[[nodiscard]] int gl_projection() noexcept;
[[nodiscard]] int gl_texture() noexcept;

// State caps
[[nodiscard]] int gl_depth_test() noexcept;
[[nodiscard]] int gl_blend() noexcept;
[[nodiscard]] int gl_alpha_test() noexcept;
[[nodiscard]] int gl_cull_face() noexcept;
[[nodiscard]] int gl_lighting() noexcept;
[[nodiscard]] int gl_fog() noexcept;
[[nodiscard]] int gl_texture_2d() noexcept;

// Face selection
[[nodiscard]] int gl_front() noexcept;
[[nodiscard]] int gl_back() noexcept;
[[nodiscard]] int gl_front_and_back() noexcept;

// Blend factors
[[nodiscard]] int gl_src_alpha() noexcept;
[[nodiscard]] int gl_one_minus_src_alpha() noexcept;
[[nodiscard]] int gl_one() noexcept;
[[nodiscard]] int gl_zero() noexcept;

// Primitive types (for glBegin)
[[nodiscard]] int gl_lines() noexcept;
[[nodiscard]] int gl_line_strip() noexcept;
[[nodiscard]] int gl_line_loop() noexcept;
[[nodiscard]] int gl_triangles() noexcept;
[[nodiscard]] int gl_triangle_strip() noexcept;
[[nodiscard]] int gl_triangle_fan() noexcept;
[[nodiscard]] int gl_quads() noexcept;
[[nodiscard]] int gl_points() noexcept;
[[nodiscard]] int gl_polygon() noexcept;

// Polygon mode (for glPolygonMode)
[[nodiscard]] int gl_line() noexcept;
[[nodiscard]] int gl_fill() noexcept;

// State masks (for glPushAttrib / glPopAttrib)
[[nodiscard]] int gl_all_attrib_bits() noexcept;

} // namespace sdk::lua::bindings::constants
