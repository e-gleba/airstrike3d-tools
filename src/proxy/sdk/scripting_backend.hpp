#pragma once

#include <filesystem>
#include <memory>

namespace sdk {

// Abstract scripting backend interface.
// Implementations: sol2_backend, luabridge_backend, etc.
//
// This is the "spec" — any script backend must implement these operations.
class scripting_backend {
public:
    virtual ~scripting_backend() = default;

    // Initialize the script runtime (create state, open libraries)
    virtual void initialize() = 0;

    // Register all SDK bindings (gl_*, ui_*, etc.)
    virtual void register_bindings() = 0;

    // Load and execute all .lua files from directory
    virtual void load_plugins(const std::filesystem::path& directory) = 0;

    // Shutdown the script runtime (cleanup, free resources)
    virtual void shutdown() = 0;
};

// Set the active scripting backend (takes ownership).
// Call this before install_hooks().
void set_scripting_backend(std::unique_ptr<scripting_backend> backend);

// Get the active scripting backend (may be null if not set).
scripting_backend* get_scripting_backend();

} // namespace sdk
