/// @file bindings.hpp
/// @brief Interface for Lua binding modules.
///
/// This header defines the common interface that all binding modules must
/// implement. Each binding module registers a set of functions and constants
/// with the Lua state, organized by domain (math, UI, SDK, etc.).
///
/// @note This is part of the private implementation. Client code should not
///       include this header directly.

#pragma once

#include <sol/sol.hpp>

namespace sdk::lua::bindings
{

/// @brief Register SDK core bindings (callbacks, GL functions, input).
/// @param state Reference to the sol2 Lua state.
/// @details Registers functions in the "sdk" namespace:
///          - Event callbacks (on_frame, on_overlay, etc.)
///          - OpenGL wrappers (gl_enable, gl_disable, etc.)
///          - Input functions (is_key_down, get_cursor_pos, etc.)
///          - Logging functions (log_info, log_warn, log_error)
void register_sdk(sol::state& state);

/// @brief Register UI bindings (ImGui wrappers).
/// @param state Reference to the sol2 Lua state.
/// @details Registers functions in the "ui" namespace:
///          - Window management (begin_window, end_window)
///          - Text display (text, text_colored, etc.)
///          - Input widgets (button, checkbox, slider, etc.)
///          - Layout functions (separator, same_line, columns, etc.)
void register_ui(sol::state& state);

/// @brief Register math bindings (vector and matrix operations).
/// @param state Reference to the sol2 Lua state.
/// @details Registers functions in the "gmath" namespace:
///          - Trigonometric functions (sin, cos, radians)
///          - Vector operations (normalize, cross)
///          - Matrix operations (lookat_matrix)
///          - Utility functions (clamp, mod)
void register_math(sol::state& state);

/// @brief Register constant bindings (virtual keys, GL constants).
/// @param state Reference to the sol2 Lua state.
/// @details Registers constant tables:
///          - "VK" table: Virtual key codes (VK_SHIFT, VK_A, VK_F1, etc.)
///          - "GL" table: OpenGL constants (GL_TRIANGLES, GL_BLEND, etc.)
void register_constants(sol::state& state);

} // namespace sdk::lua::bindings
