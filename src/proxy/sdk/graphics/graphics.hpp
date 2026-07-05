/// @file graphics.hpp
/// @brief Graphics state and drawing helpers (backend-agnostic public interface).

#pragma once

#include <cstdint>

namespace sdk::graphics
{

void enable(std::int32_t cap);
void disable(std::int32_t cap);
void depth_mask(bool flag);
void blend_func(std::int32_t sfactor, std::int32_t dfactor);
void line_width(float w);
void point_size(float sz);
void color4f(float r, float g, float b, float a);
void color3f(float r, float g, float b);
void polygon_mode(std::int32_t face, std::int32_t mode);
void push_attrib(std::int32_t mask);
void pop_attrib();
void push_matrix();
void pop_matrix();
void begin(std::int32_t mode);
void end();
void vertex3f(float x, float y, float z);
void vertex2f(float x, float y);
void translate(double x, double y, double z);
void rotate(double angle, double x, double y, double z);
void scale(double x, double y, double z);
void mult_matrix(const double* m16);
void apply_lookat(double ex, double ey, double ez,
                  double cx, double cy, double cz,
                  double ux, double uy, double uz);

namespace constants
{

[[nodiscard]] std::int32_t vk_shift();
[[nodiscard]] std::int32_t vk_control();
[[nodiscard]] std::int32_t vk_space();
[[nodiscard]] std::int32_t vk_insert();
[[nodiscard]] std::int32_t vk_escape();
[[nodiscard]] std::int32_t vk_tab();
[[nodiscard]] std::int32_t vk_return();
[[nodiscard]] std::int32_t vk_back();
[[nodiscard]] std::int32_t vk_delete();
[[nodiscard]] std::int32_t vk_home();
[[nodiscard]] std::int32_t vk_end();
[[nodiscard]] std::int32_t vk_prior();
[[nodiscard]] std::int32_t vk_next();
[[nodiscard]] std::int32_t vk_left();
[[nodiscard]] std::int32_t vk_right();
[[nodiscard]] std::int32_t vk_up();
[[nodiscard]] std::int32_t vk_down();
[[nodiscard]] std::int32_t vk_f1();
[[nodiscard]] std::int32_t vk_f2();
[[nodiscard]] std::int32_t vk_f3();
[[nodiscard]] std::int32_t vk_f4();
[[nodiscard]] std::int32_t vk_f5();
[[nodiscard]] std::int32_t vk_f6();
[[nodiscard]] std::int32_t vk_f7();
[[nodiscard]] std::int32_t vk_f8();
[[nodiscard]] std::int32_t vk_f9();
[[nodiscard]] std::int32_t vk_f10();
[[nodiscard]] std::int32_t vk_f11();
[[nodiscard]] std::int32_t vk_f12();
[[nodiscard]] std::int32_t vk_lbutton();
[[nodiscard]] std::int32_t vk_rbutton();
[[nodiscard]] std::int32_t vk_mbutton();
[[nodiscard]] std::int32_t vk_w();
[[nodiscard]] std::int32_t vk_a();
[[nodiscard]] std::int32_t vk_s();
[[nodiscard]] std::int32_t vk_d();
[[nodiscard]] std::int32_t vk_q();
[[nodiscard]] std::int32_t vk_e();
[[nodiscard]] std::int32_t vk_c();
[[nodiscard]] std::int32_t vk_r();
[[nodiscard]] std::int32_t vk_z();
[[nodiscard]] std::int32_t vk_x();
[[nodiscard]] std::int32_t vk_v();

[[nodiscard]] std::int32_t gl_modelview();
[[nodiscard]] std::int32_t gl_projection();
[[nodiscard]] std::int32_t gl_texture();
[[nodiscard]] std::int32_t gl_depth_test();
[[nodiscard]] std::int32_t gl_blend();
[[nodiscard]] std::int32_t gl_alpha_test();
[[nodiscard]] std::int32_t gl_cull_face();
[[nodiscard]] std::int32_t gl_lighting();
[[nodiscard]] std::int32_t gl_fog();
[[nodiscard]] std::int32_t gl_texture_2d();
[[nodiscard]] std::int32_t gl_front();
[[nodiscard]] std::int32_t gl_back();
[[nodiscard]] std::int32_t gl_front_and_back();
[[nodiscard]] std::int32_t gl_src_alpha();
[[nodiscard]] std::int32_t gl_one_minus_src_alpha();
[[nodiscard]] std::int32_t gl_one();
[[nodiscard]] std::int32_t gl_zero();
[[nodiscard]] std::int32_t gl_lines();
[[nodiscard]] std::int32_t gl_line_strip();
[[nodiscard]] std::int32_t gl_line_loop();
[[nodiscard]] std::int32_t gl_triangles();
[[nodiscard]] std::int32_t gl_triangle_strip();
[[nodiscard]] std::int32_t gl_triangle_fan();
[[nodiscard]] std::int32_t gl_quads();
[[nodiscard]] std::int32_t gl_points();
[[nodiscard]] std::int32_t gl_polygon();
[[nodiscard]] std::int32_t gl_line();
[[nodiscard]] std::int32_t gl_fill();
[[nodiscard]] std::int32_t gl_all_attrib_bits();

} // namespace constants

} // namespace sdk::graphics
