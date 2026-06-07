#pragma once

// sdk/render/types.hpp — Render type definitions (pure C++23)
//
// No GL/gl.h in public header. OpenGL constants replicated as constexpr.

#include <cstdint>

namespace sdk::render
{

// ─── OpenGL constants (replicated, no GL/gl.h dependency) ───────────────

namespace gl
{

// Matrix modes
inline constexpr int k_modelview         = 0x1700;
inline constexpr int k_projection        = 0x1701;
inline constexpr int k_texture           = 0x1702;

// Enable/disable caps
inline constexpr int k_depth_test        = 0x0B71;
inline constexpr int k_blend             = 0x0BE2;
inline constexpr int k_alpha_test        = 0x0BC0;
inline constexpr int k_cull_face         = 0x0B44;
inline constexpr int k_lighting          = 0x0B50;
inline constexpr int k_fog               = 0x0B60;
inline constexpr int k_texture_2d        = 0x0DE1;
inline constexpr int k_normalize         = 0x0BA1;
inline constexpr int k_color_material    = 0x0B57;
inline constexpr int k_scissor_test      = 0x0C11;
inline constexpr int k_stencil_test      = 0x0B90;

// Face culling
inline constexpr int k_front             = 0x0404;
inline constexpr int k_back              = 0x0405;
inline constexpr int k_front_and_back    = 0x0408;

// Blend factors
inline constexpr int k_zero              = 0;
inline constexpr int k_one               = 1;
inline constexpr int k_src_alpha         = 0x0302;
inline constexpr int k_one_minus_src_alpha = 0x0303;

// Primitive types
inline constexpr int k_points            = 0x0000;
inline constexpr int k_lines             = 0x0001;
inline constexpr int k_line_loop         = 0x0002;
inline constexpr int k_line_strip        = 0x0003;
inline constexpr int k_triangles         = 0x0004;
inline constexpr int k_triangle_strip    = 0x0005;
inline constexpr int k_triangle_fan      = 0x0006;
inline constexpr int k_quads             = 0x0007;
inline constexpr int k_polygon           = 0x0009;

// Polygon modes
inline constexpr int k_point             = 0x1B00;
inline constexpr int k_line              = 0x1B01;
inline constexpr int k_fill              = 0x1B02;

// Attribute bits
inline constexpr unsigned int k_current_bit       = 0x00000001;
inline constexpr unsigned int k_point_bit         = 0x00000002;
inline constexpr unsigned int k_line_bit          = 0x00000004;
inline constexpr unsigned int k_polygon_bit       = 0x00000008;
inline constexpr unsigned int k_polygon_stipple_bit = 0x00000010;
inline constexpr unsigned int k_pixel_mode_bit    = 0x00000020;
inline constexpr unsigned int k_lighting_bit      = 0x00000040;
inline constexpr unsigned int k_fog_bit           = 0x00000080;
inline constexpr unsigned int k_depth_buffer_bit  = 0x00000100;
inline constexpr unsigned int k_accum_buffer_bit  = 0x00000200;
inline constexpr unsigned int k_stencil_buffer_bit = 0x00000400;
inline constexpr unsigned int k_viewport_bit      = 0x00000800;
inline constexpr unsigned int k_transform_bit     = 0x00001000;
inline constexpr unsigned int k_enable_bit        = 0x00002000;
inline constexpr unsigned int k_color_buffer_bit  = 0x00004000;
inline constexpr unsigned int k_hint_bit          = 0x00008000;
inline constexpr unsigned int k_eval_bit          = 0x00010000;
inline constexpr unsigned int k_list_bit          = 0x00020000;
inline constexpr unsigned int k_texture_bit       = 0x00040000;
inline constexpr unsigned int k_scissor_bit       = 0x00080000;
inline constexpr unsigned int k_all_attrib_bits   = 0x000FFFFF;

} // namespace gl

// ─── Render device context (opaque) ──────────────────────────────────────

struct device_context;

} // namespace sdk::render
