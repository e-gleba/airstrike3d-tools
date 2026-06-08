// src/proxy/sdk/lua/detail/callback_impl.cpp
// Implementation of callback_list and consuming_callbacks.

#include "sdk/lua/callback.hpp"

#include <mutex>
#include <vector>

namespace sdk::lua
{

// ─────────────────────────────────────────────────────────────────────────────
// callback_list

struct callback_list::impl
{
    std::mutex          mtx;
    std::vector<fn_type> callbacks;
};

callback_list::callback_list() : pimpl(std::make_unique<impl>()) {}
callback_list::~callback_list() = default;

callback_list::callback_list(callback_list&&) noexcept = default;
callback_list& callback_list::operator=(callback_list&&) noexcept = default;

void callback_list::add(fn_type fn)
{
    std::scoped_lock lock(pimpl->mtx);
    pimpl->callbacks.push_back(std::move(fn));
}

void callback_list::invoke()
{
    std::scoped_lock lock(pimpl->mtx);
    for (auto& cb : pimpl->callbacks) {
        cb();
    }
}

void callback_list::clear()
{
    std::scoped_lock lock(pimpl->mtx);
    pimpl->callbacks.clear();
}

bool callback_list::empty() const
{
    std::scoped_lock lock(pimpl->mtx);
    return pimpl->callbacks.empty();
}

// ─────────────────────────────────────────────────────────────────────────────
// consuming_callbacks

struct consuming_callbacks::impl
{
    std::mutex          mtx;
    std::vector<fn_type> callbacks;
};

consuming_callbacks::consuming_callbacks() : pimpl(std::make_unique<impl>()) {}
consuming_callbacks::~consuming_callbacks() = default;

consuming_callbacks::consuming_callbacks(consuming_callbacks&&) noexcept = default;
consuming_callbacks& consuming_callbacks::operator=(consuming_callbacks&&) noexcept = default;

void consuming_callbacks::add(fn_type fn)
{
    std::scoped_lock lock(pimpl->mtx);
    pimpl->callbacks.push_back(std::move(fn));
}

bool consuming_callbacks::invoke()
{
    std::scoped_lock lock(pimpl->mtx);
    for (auto& cb : pimpl->callbacks) {
        if (cb()) {
            return true; // consumed
        }
    }
    return false;
}

void consuming_callbacks::clear()
{
    std::scoped_lock lock(pimpl->mtx);
    pimpl->callbacks.clear();
}

bool consuming_callbacks::empty() const
{
    std::scoped_lock lock(pimpl->mtx);
    return pimpl->callbacks.empty();
}

} // namespace sdk::lua
