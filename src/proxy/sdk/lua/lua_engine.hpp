#pragma once

#include <memory>

namespace sdk::lua
{

/// @brief RAII Lua scripting engine.
///
/// Thin C++23 wrapper that isolates all sol2 / Lua types behind a pimpl
/// boundary.  Users interact only with standard C++ types; swapping the
/// binding backend (sol2 → LuaBridge3, etc.) requires changes only in
/// the private implementation.
///
/// Lifetime:
///   - Construction initialises the Lua state and registers all bindings.
///   - load_plugins() scans the plugin directory and executes .lua scripts.
///   - Destruction automatically unloads plugins and releases the state.
///
/// Thread-safety:
///   - Callback invocation is serialised through an internal mutex.
///   - load_plugins / unload_plugins must not be called concurrently.
class engine
{
public:
    engine();
    ~engine();

    engine(engine&&) noexcept;
    engine& operator=(engine&&) noexcept;

    engine(const engine&)            = delete;
    engine& operator=(const engine&) = delete;

    /// @brief Scan plugin directory and execute all .lua scripts.
    void load_plugins();

    /// @brief Invoke on_unload callbacks and release the Lua state.
    void unload_plugins();

    // ── Callback invocation (called from C++ hooks) ─────────────────────────

    void invoke_on_frame();
    void invoke_on_overlay();
    void invoke_on_gl_identity();
    void invoke_on_glu_lookat();

    /// @brief Invoke on_key_down callbacks.
    /// @return true if any callback consumed the event.
    [[nodiscard]] bool invoke_on_key_down(int key);

    void invoke_on_load();
    void invoke_on_unload();

    // ── Queries ─────────────────────────────────────────────────────────────

    /// @brief true if at least one plugin was loaded successfully.
    [[nodiscard]] bool has_plugins() const;

private:
    struct impl;
    std::unique_ptr<impl> pimpl_;
};

} // namespace sdk::lua
