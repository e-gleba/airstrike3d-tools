#pragma once

#include <filesystem>
#include <memory>
#include <string_view>

namespace sdk
{

/// Opaque Lua scripting engine. Pimpl pattern hides sol2/Lua implementation entirely.
///
/// RAII: constructs Lua state on creation, destroys on scope exit.
/// Move-only: unique ownership of the Lua VM.
///
/// Public API exposes zero Lua/sol2 types. All bindings registered internally.
/// Consumers interact through type-safe C++ interfaces only.
///
/// Usage:
///   script_engine engine;
///   engine.register_bindings();
///   engine.load_plugins("plugins");
///   engine.invoke("on_load");
///
class script_engine
{
public:
    script_engine();
    ~script_engine() noexcept;

    script_engine(const script_engine&)            = delete;
    script_engine& operator=(const script_engine&) = delete;

    script_engine(script_engine&&) noexcept;
    script_engine& operator=(script_engine&&) noexcept;

    /// Register all SDK bindings (sdk, ui, math, constants tables).
    void register_bindings();

    /// Load and execute all .lua files from plugin_dir, sorted alphabetically.
    /// Creates directory if missing. Logs errors per-script, continues loading.
    void load_plugins(const std::filesystem::path& plugin_dir);

    /// Invoke Lua callbacks registered under event_name. Args forwarded to Lua.
    /// No-op if Lua state is invalid or callback not registered.
    void invoke(std::string_view event_name);

    template <typename... Args>
    void invoke(std::string_view event_name, Args&&... args);

    /// Check if Lua state is valid (post-construction, pre-destruction).
    [[nodiscard]] explicit operator bool() const noexcept;

private:
    struct impl;
    std::unique_ptr<impl> pimpl;

    void invoke_impl(std::string_view event_name, auto&&... args);
};

// ─── Template implementation (must be in header) ─────────────────────────────

template <typename... Args>
void script_engine::invoke(std::string_view event_name, Args&&... args)
{
    invoke_impl(event_name, std::forward<Args>(args)...);
}

} // namespace sdk
