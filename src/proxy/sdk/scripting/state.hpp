#pragma once

// sdk/scripting/state.hpp — Abstract scripting state interface (pure C++23)
//
// Polukhin-style: PIMPL, no sol2 in public header.
// Turner-style: noexcept where possible, std::expected for errors.
//
// This interface defines the Lua API spec independently of sol2 backend.
// Alternative backends (LuaBridge3, etc.) implement this same interface.

#include <expected>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace sdk::scripting
{

// ─── Error type ───────────────────────────────────────────────────────────

struct script_error
{
    std::string message;
    std::string source_file;
    int         line{};
};

// ─── Callback types (type-erased, no sol2) ────────────────────────────────

using void_callback     = std::function<void()>;
using bool_callback     = std::function<bool()>;
using int_callback      = std::function<bool(int)>;
using string_callback   = std::function<void(std::string_view)>;

// ─── SDK function signatures (exposed to Lua) ────────────────────────────

using sdk_log_fn        = std::function<void(std::string_view)>;
using sdk_gl_enable_fn  = std::function<void(int)>;
using sdk_gl_disable_fn = std::function<void(int)>;
using sdk_gl_color_fn   = std::function<void(float, float, float, float)>;
using sdk_gl_vertex_fn  = std::function<void(float, float, float)>;

// ─── Abstract state interface ─────────────────────────────────────────────

class state
{
public:
    virtual ~state() = default;

    // Script execution
    virtual auto execute_file(std::filesystem::path const& path) 
        -> std::expected<void, script_error> = 0;

    virtual auto execute_string(std::string_view code) 
        -> std::expected<void, script_error> = 0;

    // Callback registration (type-erased)
    virtual void register_callback(std::string_view event_name, void_callback fn) = 0;
    virtual void register_key_callback(std::string_view event_name, int_callback fn) = 0;

    // Callback invocation
    virtual void invoke_callbacks(std::string_view event_name) = 0;
    virtual auto invoke_key_callbacks(std::string_view event_name, int key) -> bool = 0;

    // SDK function registration (called by bindings layer)
    virtual void register_sdk_function(std::string_view name, std::function<void()> fn) = 0;
    virtual void register_math_function(std::string_view name, std::function<void()> fn) = 0;
    virtual void register_ui_function(std::string_view name, std::function<void()> fn) = 0;

    // Constant registration
    virtual void register_int_constant(std::string_view table, std::string_view name, int value) = 0;
    virtual void register_float_constant(std::string_view table, std::string_view name, double value) = 0;

    // Lifecycle
    virtual void clear_all_callbacks() = 0;

    // Factory
    [[nodiscard]] static auto create() -> std::unique_ptr<state>;
};

} // namespace sdk::scripting
