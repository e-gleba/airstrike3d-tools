#pragma once

// sdk/hooking/hook.hpp — Abstract inline hook interface (pure C++23)
//
// Polukhin-style: PIMPL, no safetyhook in public header.
// Turner-style: noexcept, RAII.

#include <functional>
#include <memory>
#include <string_view>

namespace sdk::hooking
{

// ─── Hook handle (opaque, RAII) ───────────────────────────────────────────

class inline_hook
{
public:
    inline_hook();
    ~inline_hook();

    inline_hook(inline_hook const&)            = delete;
    auto operator=(inline_hook const&) -> inline_hook& = delete;

    inline_hook(inline_hook&&) noexcept;
    auto operator=(inline_hook&&) noexcept -> inline_hook&;

    // Create hook at target address, redirecting to detour
    [[nodiscard]] static auto create(void* target, void* detour) -> inline_hook;

    // Get original function (trampoline)
    [[nodiscard]] auto trampoline() const noexcept -> void*;

    // Check if hook is active
    [[nodiscard]] auto is_active() const noexcept -> bool;

    // Disable hook
    void reset();

private:
    struct impl;
    std::unique_ptr<impl> pimpl_;

    explicit inline_hook(std::unique_ptr<impl> p) noexcept;
};

// ─── Hook registry ───────────────────────────────────────────────────────

class hook_registry
{
public:
    hook_registry();
    ~hook_registry();

    hook_registry(hook_registry const&)            = delete;
    auto operator=(hook_registry const&) -> hook_registry& = delete;

    // Install hook by module and procedure name
    void install(std::wstring_view module_name, std::string_view proc_name, void* detour);

    // Install hook at specific address
    void install_at(void* target, void* detour);

    // Reset all hooks
    void reset_all();

private:
    struct impl;
    std::unique_ptr<impl> pimpl_;
};

} // namespace sdk::hooking
