// sdk/render/opengl_hooks.cpp — OpenGL hook implementations
//
// All GL/gl.h code isolated here. Public header remains pure C++23.

#include "sdk/render/opengl_hooks.hpp"
#include "sdk/render/types.hpp"

#include <GL/gl.h>
#include <GL/glu.h>

namespace sdk::render
{

// ─── State tracking ──────────────────────────────────────────────────────

namespace
{

int g_current_matrix_mode = gl::k_modelview;

} // namespace

// ─── Hook callbacks ──────────────────────────────────────────────────────

void on_gl_matrix_mode(int mode) noexcept
{
    g_current_matrix_mode = mode;
}

void on_gl_load_identity() noexcept
{
    // State tracking for matrix mode changes
    // Actual GL call happens in trampoline
}

void on_glu_look_at(double eye_x, double eye_y, double eye_z,
                    double center_x, double center_y, double center_z,
                    double up_x, double up_y, double up_z) noexcept
{
    // Callback for camera tracking
    // Actual GL call happens in trampoline
}

} // namespace sdk::render
