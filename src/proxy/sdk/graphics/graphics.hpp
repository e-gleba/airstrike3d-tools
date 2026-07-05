/// @file graphics.hpp
/// @brief Graphics state and drawing helpers (backend-agnostic public interface).

#pragma once

#include <cstdint>
#include <span>

namespace sdk::graphics
{

void enable(std::int32_t cap) noexcept;
void disable(std::int32_t cap) noexcept;
void depth_mask(bool flag) noexcept;
void blend_func(std::int32_t sfactor, std::int32_t dfactor) noexcept;
void line_width(float w) noexcept;
void point_size(float sz) noexcept;
void color4f(float r, float g, float b, float a) noexcept;
void color3f(float r, float g, float b) noexcept;
void polygon_mode(std::int32_t face, std::int32_t mode) noexcept;
void push_attrib(std::int32_t mask) noexcept;
void pop_attrib() noexcept;
void push_matrix() noexcept;
void pop_matrix() noexcept;
void begin(std::int32_t mode) noexcept;
void end() noexcept;
void vertex3f(float x, float y, float z) noexcept;
void vertex2f(float x, float y) noexcept;
void translate(double x, double y, double z) noexcept;
void rotate(double angle, double x, double y, double z) noexcept;
void scale(double x, double y, double z) noexcept;
void mult_matrix(std::span<const double, 16> matrix);
void apply_lookat(double ex, double ey, double ez,
                  double cx, double cy, double cz,
                  double ux, double uy, double uz) noexcept;

namespace constants
{

[[nodiscard]] constexpr std::int32_t vk_shift() noexcept { return 0x10; }
[[nodiscard]] constexpr std::int32_t vk_control() noexcept { return 0x11; }
[[nodiscard]] constexpr std::int32_t vk_space() noexcept { return 0x20; }
[[nodiscard]] constexpr std::int32_t vk_insert() noexcept { return 0x2D; }
[[nodiscard]] constexpr std::int32_t vk_escape() noexcept { return 0x1B; }
[[nodiscard]] constexpr std::int32_t vk_tab() noexcept { return 0x09; }
[[nodiscard]] constexpr std::int32_t vk_return() noexcept { return 0x0D; }
[[nodiscard]] constexpr std::int32_t vk_back() noexcept { return 0x08; }
[[nodiscard]] constexpr std::int32_t vk_delete() noexcept { return 0x2E; }
[[nodiscard]] constexpr std::int32_t vk_home() noexcept { return 0x24; }
[[nodiscard]] constexpr std::int32_t vk_end() noexcept { return 0x23; }
[[nodiscard]] constexpr std::int32_t vk_prior() noexcept { return 0x21; }
[[nodiscard]] constexpr std::int32_t vk_next() noexcept { return 0x22; }
[[nodiscard]] constexpr std::int32_t vk_left() noexcept { return 0x25; }
[[nodiscard]] constexpr std::int32_t vk_right() noexcept { return 0x26; }
[[nodiscard]] constexpr std::int32_t vk_up() noexcept { return 0x27; }
[[nodiscard]] constexpr std::int32_t vk_down() noexcept { return 0x28; }
[[nodiscard]] constexpr std::int32_t vk_f1() noexcept { return 0x70; }
[[nodiscard]] constexpr std::int32_t vk_f2() noexcept { return 0x71; }
[[nodiscard]] constexpr std::int32_t vk_f3() noexcept { return 0x72; }
[[nodiscard]] constexpr std::int32_t vk_f4() noexcept { return 0x73; }
[[nodiscard]] constexpr std::int32_t vk_f5() noexcept { return 0x74; }
[[nodiscard]] constexpr std::int32_t vk_f6() noexcept { return 0x75; }
[[nodiscard]] constexpr std::int32_t vk_f7() noexcept { return 0x76; }
[[nodiscard]] constexpr std::int32_t vk_f8() noexcept { return 0x77; }
[[nodiscard]] constexpr std::int32_t vk_f9() noexcept { return 0x78; }
[[nodiscard]] constexpr std::int32_t vk_f10() noexcept { return 0x79; }
[[nodiscard]] constexpr std::int32_t vk_f11() noexcept { return 0x7A; }
[[nodiscard]] constexpr std::int32_t vk_f12() noexcept { return 0x7B; }
[[nodiscard]] constexpr std::int32_t vk_lbutton() noexcept { return 0x01; }
[[nodiscard]] constexpr std::int32_t vk_rbutton() noexcept { return 0x02; }
[[nodiscard]] constexpr std::int32_t vk_mbutton() noexcept { return 0x04; }
[[nodiscard]] constexpr std::int32_t vk_w() noexcept { return 'W'; }
[[nodiscard]] constexpr std::int32_t vk_a() noexcept { return 'A'; }
[[nodiscard]] constexpr std::int32_t vk_s() noexcept { return 'S'; }
[[nodiscard]] constexpr std::int32_t vk_d() noexcept { return 'D'; }
[[nodiscard]] constexpr std::int32_t vk_q() noexcept { return 'Q'; }
[[nodiscard]] constexpr std::int32_t vk_e() noexcept { return 'E'; }
[[nodiscard]] constexpr std::int32_t vk_c() noexcept { return 'C'; }
[[nodiscard]] constexpr std::int32_t vk_r() noexcept { return 'R'; }
[[nodiscard]] constexpr std::int32_t vk_z() noexcept { return 'Z'; }
[[nodiscard]] constexpr std::int32_t vk_x() noexcept { return 'X'; }
[[nodiscard]] constexpr std::int32_t vk_v() noexcept { return 'V'; }

[[nodiscard]] constexpr std::int32_t gl_modelview() noexcept { return 0x1700; }
[[nodiscard]] constexpr std::int32_t gl_projection() noexcept { return 0x1701; }
[[nodiscard]] constexpr std::int32_t gl_texture() noexcept { return 0x1702; }
[[nodiscard]] constexpr std::int32_t gl_depth_test() noexcept { return 0x0B71; }
[[nodiscard]] constexpr std::int32_t gl_blend() noexcept { return 0x0BE2; }
[[nodiscard]] constexpr std::int32_t gl_alpha_test() noexcept { return 0x0BC0; }
[[nodiscard]] constexpr std::int32_t gl_cull_face() noexcept { return 0x0B44; }
[[nodiscard]] constexpr std::int32_t gl_lighting() noexcept { return 0x0B50; }
[[nodiscard]] constexpr std::int32_t gl_fog() noexcept { return 0x0B60; }
[[nodiscard]] constexpr std::int32_t gl_texture_2d() noexcept { return 0x0DE1; }
[[nodiscard]] constexpr std::int32_t gl_front() noexcept { return 0x0404; }
[[nodiscard]] constexpr std::int32_t gl_back() noexcept { return 0x0405; }
[[nodiscard]] constexpr std::int32_t gl_front_and_back() noexcept { return 0x0408; }
[[nodiscard]] constexpr std::int32_t gl_src_alpha() noexcept { return 0x0302; }
[[nodiscard]] constexpr std::int32_t gl_one_minus_src_alpha() noexcept { return 0x0303; }
[[nodiscard]] constexpr std::int32_t gl_one() noexcept { return 1; }
[[nodiscard]] constexpr std::int32_t gl_zero() noexcept { return 0; }
[[nodiscard]] constexpr std::int32_t gl_lines() noexcept { return 0x0001; }
[[nodiscard]] constexpr std::int32_t gl_line_strip() noexcept { return 0x0003; }
[[nodiscard]] constexpr std::int32_t gl_line_loop() noexcept { return 0x0002; }
[[nodiscard]] constexpr std::int32_t gl_triangles() noexcept { return 0x0004; }
[[nodiscard]] constexpr std::int32_t gl_triangle_strip() noexcept { return 0x0005; }
[[nodiscard]] constexpr std::int32_t gl_triangle_fan() noexcept { return 0x0006; }
[[nodiscard]] constexpr std::int32_t gl_quads() noexcept { return 0x0007; }
[[nodiscard]] constexpr std::int32_t gl_points() noexcept { return 0x0000; }
[[nodiscard]] constexpr std::int32_t gl_polygon() noexcept { return 0x0009; }
[[nodiscard]] constexpr std::int32_t gl_line() noexcept { return 0x1B01; }
[[nodiscard]] constexpr std::int32_t gl_fill() noexcept { return 0x1B02; }
[[nodiscard]] constexpr std::int32_t gl_all_attrib_bits() noexcept { return 0x000FFFFF; }

} // namespace constants

} // namespace sdk::graphics
