// sdk/hooking/safetyhook_backend.cpp — safetyhook implementation
//
// ALL safetyhook code isolated here. Public header remains pure C++23.

#include "sdk/hooking/hook.hpp"
#include "sdk/platform/module.hpp"

#include <safetyhook.hpp>
#include <spdlog/spdlog.h>

#include <vector>

namespace sdk::hooking
{

// ─── inline_hook::impl ────────────────────────────────────────────────────

struct inline_hook::impl
{
    safetyhook::InlineHook hook;

    impl() = default;
    impl(safetyhook::InlineHook h) : hook(std::move(h)) {}
};

// ─── inline_hook implementation ──────────────────────────────────────────

inline_hook::inline_hook() : pimpl_(std::make_unique<impl>()) {}

inline_hook::~inline_hook() = default;

inline_hook::inline_hook(inline_hook&&) noexcept = default;
auto inline_hook::operator=(inline_hook&&) noexcept -> inline_hook& = default;

inline_hook::inline_hook(std::unique_ptr<impl> p) noexcept : pimpl_(std::move(p)) {}

auto inline_hook::create(void* target, void* detour) -> inline_hook
{
    auto hook = safetyhook::create_inline(target, detour);
    return inline_hook(std::make_unique<impl>(std::move(hook)));
}

auto inline_hook::trampoline() const noexcept -> void*
{
    return pimpl_ ? reinterpret_cast<void*>(pimpl_->hook.trampoline().address()) : nullptr;
}

auto inline_hook::is_active() const noexcept -> bool
{
    return pimpl_ && pimpl_->hook;
}

void inline_hook::reset()
{
    if (pimpl_)
    {
        pimpl_->hook.reset();
    }
}

// ─── hook_registry::impl ─────────────────────────────────────────────────

struct hook_registry::impl
{
    std::vector<inline_hook> hooks;
};

// ─── hook_registry implementation ────────────────────────────────────────

hook_registry::hook_registry() : pimpl_(std::make_unique<impl>()) {}

hook_registry::~hook_registry() = default;

void hook_registry::install(std::wstring_view module_name, std::string_view proc_name, void* detour)
{
    auto mod = platform::get_module(module_name);
    if (!mod)
    {
        spdlog::error("[hooking] module not found: {}", std::wstring(module_name.begin(), module_name.end()));
        return;
    }

    auto proc = platform::get_proc_address(mod, proc_name);
    if (!proc)
    {
        spdlog::error("[hooking] procedure not found: {}", proc_name);
        return;
    }

    auto hook = inline_hook::create(proc, detour);
    pimpl_->hooks.push_back(std::move(hook));
    spdlog::debug("[hooking] installed hook: {}!{}", 
                  std::wstring(module_name.begin(), module_name.end()), 
                  proc_name);
}

void hook_registry::install_at(void* target, void* detour)
{
    if (!target || !detour)
    {
        spdlog::error("[hooking] invalid target or detour");
        return;
    }

    auto hook = inline_hook::create(target, detour);
    pimpl_->hooks.push_back(std::move(hook));
    spdlog::debug("[hooking] installed hook at address");
}

void hook_registry::reset_all()
{
    for (auto& hook : pimpl_->hooks)
    {
        hook.reset();
    }
    pimpl_->hooks.clear();
    spdlog::debug("[hooking] all hooks reset");
}

} // namespace sdk::hooking
