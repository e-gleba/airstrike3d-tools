#pragma once

// sdk/sdk.hpp — Root SDK header (pure C++23)
//
// This is the only header users need to include.
// All external dependencies (sol2, WinAPI, GLM, OpenGL, safetyhook, ImGui)
// are hidden behind abstraction layers.

#include "sdk/api/math_types.hpp"
#include "sdk/api/render_api.hpp"
#include "sdk/api/callbacks.hpp"
#include "sdk/api/key_codes.hpp"

#include "sdk/core/engine.hpp"
#include "sdk/core/logging.hpp"

#include "sdk/platform/types.hpp"
#include "sdk/platform/window.hpp"
#include "sdk/platform/module.hpp"
#include "sdk/platform/input.hpp"

#include "sdk/math/vec3.hpp"
#include "sdk/math/mat4.hpp"
#include "sdk/math/operations.hpp"

#include "sdk/scripting/state.hpp"
#include "sdk/scripting/callback_registry.hpp"

#include "sdk/hooking/hook.hpp"

#include "sdk/render/types.hpp"
#include "sdk/render/opengl_hooks.hpp"

#include "sdk/overlay/overlay.hpp"

namespace sdk
{

// ─── Version ──────────────────────────────────────────────────────────────

inline constexpr int k_version_major = 2;
inline constexpr int k_version_minor = 0;
inline constexpr int k_version_patch = 0;

// ─── Convenience aliases ──────────────────────────────────────────────────

using engine = core::engine;
using config = core::config;

} // namespace sdk
