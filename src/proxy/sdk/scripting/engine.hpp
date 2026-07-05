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
    engine();
    ~engine();

    engine(const engine&)            = delete;
    engine& operator=(const engine&) = delete;
    engine(engine&&) noexcept;
    engine& operator=(engine&&) noexcept;

    /// Load all scripts from the configured plugins directory.
    /// Invokes the `on_load` callback after successful loads.
    void load_plugins();

    /// Unload all plugins, invoke `on_unload`, and clear registered callbacks.
    void unload_plugins();

private:
    struct impl;
    std::unique_ptr<impl> pimpl_;
};

} // namespace sdk::scripting
