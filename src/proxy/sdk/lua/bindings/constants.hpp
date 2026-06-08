/// @file bindings/constants.hpp
/// @brief Pure C++ constant accessors for Lua binding (no sol2 types).

#pragma once

namespace sdk::lua::bindings::constants
{

// Virtual keys
[[nodiscard]] int vk_shift() noexcept;
[[nodiscard]] int vk_control() noexcept;
[[nodiscard]] int vk_space() noexcept;
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
[[nodiscard]] int vk_left() noexcept;
[[nodiscard]] int vk_right() noexcept;
[[nodiscard]] int vk_up() noexcept;
[[nodiscard]] int vk_down() noexcept;

// OpenGL constants
[[nodiscard]] int gl_modelview() noexcept;
[[nodiscard]] int gl_projection() noexcept;
[[nodiscard]] int gl_texture() noexcept;
[[nodiscard]] int gl_depth_test() noexcept;
[[nodiscard]] int gl_blend() noexcept;
[[nodiscard]] int gl_alpha_test() noexcept;
[[nodiscard]] int gl_cull_face() noexcept;
[[nodiscard]] int gl_lighting() noexcept;
[[nodiscard]] int gl_fog() noexcept;
[[nodiscard]] int gl_front() noexcept;
[[nodiscard]] int gl_back() noexcept;
[[nodiscard]] int gl_front_and_back() noexcept;

// Blend factors
[[nodiscard]] int gl_src_alpha() noexcept;
[[nodiscard]] int gl_one_minus_src_alpha() noexcept;
[[nodiscard]] int gl_one() noexcept;
[[nodiscard]] int gl_zero() noexcept;

// Primitive types
[[nodiscard]] int gl_lines() noexcept;
[[nodiscard]] int gl_line_strip() noexcept;
[[nodiscard]] int gl_line_loop() noexcept;
[[nodiscard]] int gl_triangles() noexcept;
[[nodiscard]] int gl_triangle_strip() noexcept;
[[nodiscard]] int gl_triangle_fan() noexcept;
[[nodiscard]] int gl_quads() noexcept;
[[nodiscard]] int gl_points() noexcept;
[[nodiscard]] int gl_polygon() noexcept;

} // namespace sdk::lua::bindings::constants
