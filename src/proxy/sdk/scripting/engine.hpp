/// @file engine.hpp
/// @brief Script engine — public RAII wrapper (backend hidden behind pimpl).

#pragma once

#include <memory>

namespace sdk::scripting
{

/// Owns the script interpreter and plugin lifecycle.
///
/// Thread-safety: callback lists in `g_ctx.cb` use their own mutex.
/// The engine should be accessed from a single thread during runtime.
class engine final
{
public:
    /// @throws std::runtime_error if the script backend fails to initialize.
    engine();
    ~engine();

    engine(const engine&)            = delete;
    engine& operator=(const engine&) = delete;
    engine(engine&&) noexcept;
    engine& operator=(engine&&) noexcept;

    /// Load all scripts from the configured plugins directory.
    /// Per-plugin syntax errors are logged; loading continues.
    /// @throws std::logic_error if the engine was moved from.
    /// @throws std::runtime_error on fatal filesystem errors.
    void load_plugins();

    /// Unload all plugins, invoke `on_unload`, and clear registered callbacks.
    /// @throws std::logic_error if the engine was moved from.
    void unload_plugins();

private:
    struct impl;
    std::unique_ptr<impl> pimpl_;

    void require_active() const;
};

} // namespace sdk::scripting
