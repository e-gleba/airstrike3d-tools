#pragma once

// sdk/render/opengl_hooks.hpp — OpenGL hook interface (pure C++23)
//
// No GL/gl.h in public header. Functions declared with standard types.

#include <cstdint>

namespace sdk::render
{

// ─── Hook callbacks ──────────────────────────────────────────────────────

// Called when glMatrixMode is invoked
void on_gl_matrix_mode(int mode) noexcept;

// Called when glLoadIdentity is invoked
void on_gl_load_identity() noexcept;

// Called when gluLookAt is invoked
void on_glu_look_at(double eye_x, double eye_y, double eye_z,
                    double center_x, double center_y, double center_z,
                    double up_x, double up_y, double up_z) noexcept;

} // namespace sdk::render
