#pragma once
#include <GL/gl.h>
#include <windows.h>

namespace sdk::gl
{
void APIENTRY hk_gl_matrix_mode(GLenum mode);
void APIENTRY hk_gl_load_identity();
void APIENTRY hk_glu_look_at(GLdouble ex,
                             GLdouble ey,
                             GLdouble ez,
                             GLdouble cx,
                             GLdouble cy,
                             GLdouble cz,
                             GLdouble ux,
                             GLdouble uy,
                             GLdouble uz);
} // namespace sdk::gl