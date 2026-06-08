#pragma once

/// @file engine.hpp
/// @brief RAII Lua scripting engine. Internal — not part of public API.
///
/// This class manages the Lua VM lifecycle. Implementation uses sol2,
/// but the interface is backend-agnostic. To swap backends, replace
/// the impl struct in engine.cpp.

#include <filesystem>
#include <memory>

namespace sdk::lua {

/// RAII wrapper for Lua scripting engine.
///
/// Regular type with move semantics. Unique ownership of Lua VM.
/// Construction opens the VM, destruction closes it.
///
/// @invariant operator bool() == true iff VM is open
class engine {
public:
    engine();
    ~engine() noexcept;

    engine(engine&&) noexcept;
    engine& operator=(engine&&) noexcept;

    engine(const engine&) = delete;
    engine& operator=(const engine&) = delete;

    /// Open Lua state, load standard libraries.
    void initialize();

    /// Register all SDK bindings (sdk.*, ui.*, gmath.*, VK.*, GL.*).
    void register_bindings();

    /// Load and execute all .lua files from directory (sorted alphabetically).
    void load_plugins(const std::filesystem::path& directory);

    /// Close Lua state, release all resources.
    void shutdown();

    /// @return true if Lua state is open
    [[nodiscard]] explicit operator bool() const noexcept;

private:
    struct impl;
    std::unique_ptr<impl> pimpl_;
};

} // namespace sdk::lua
