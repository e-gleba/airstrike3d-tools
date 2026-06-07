#pragma once

#include <filesystem>
#include <memory>
#include <string_view>

namespace sdk
{

/// Opaque Lua scripting engine. Pimpl pattern hides sol2/Lua implementation.
///
/// RAII: constructs Lua state, destructs cleanly.
/// Move-only: no copies allowed (unique ownership of Lua VM).
///
/// Public API exposes zero Lua/sol2 types. All bindings registered internally.
///
/// Usage:
///   script_engine engine;
///   engine.register_bindings();
///   engine.load_plugins("plugins");
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
    void load_plugins(std::filesystem::path plugin_dir);

    /// Execute Lua callback by event name. Args forwarded to Lua.
    /// No-op if Lua state invalid or callback not registered.
    template <typename... Args>
    void invoke(std::string_view event_name, Args&&... args);

    /// Check if Lua state is valid (post-construction, pre-destruction).
    [[nodiscard]] explicit operator bool() const noexcept;

private:
    struct impl;
    std::unique_ptr<impl> pimpl;
};

} // namespace sdk
