#pragma once

// sdk/scripting/callbacks.hpp — Callback event names (pure C++23)

#include <string_view>

namespace sdk::scripting
{

// Callback event identifiers (for scripting layer)
inline constexpr std::string_view k_on_frame       = "on_frame";
inline constexpr std::string_view k_on_overlay     = "on_overlay";
inline constexpr std::string_view k_on_gl_identity = "on_gl_identity";
inline constexpr std::string_view k_on_glu_lookat  = "on_glu_lookat";
inline constexpr std::string_view k_on_key_down    = "on_key_down";
inline constexpr std::string_view k_on_load        = "on_load";
inline constexpr std::string_view k_on_unload      = "on_unload";

} // namespace sdk::scripting
