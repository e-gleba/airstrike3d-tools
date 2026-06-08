/// @file detail/callback_manager.cpp
/// @brief callback_manager member definitions.

#include "sdk/lua/callback_manager.hpp"
#include "sdk/lua/detail/impl.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <ranges>
#include <utility>

namespace sdk::lua
{

// ── callback_manager ─────────────────────────────────────────────────────────

callback_manager::callback_manager(std::recursive_mutex& mtx) noexcept
    : impl_{std::make_unique<impl>(mtx)}
{}

callback_manager::~callback_manager() = default;

callback_manager::callback_manager(callback_manager&&) noexcept = default;
callback_manager& callback_manager::operator=(callback_manager&&) noexcept
    = default;

void callback_manager::add(slot_fn fn)
{
    std::lock_guard lk{impl_->mutex};
    impl_->slots.push_back(std::move(fn));
}

void callback_manager::invoke()
{
    std::lock_guard lk{impl_->mutex};
    for (auto& fn : impl_->slots)
    {
        try
        {
            fn();
        }
        catch (const std::exception& e)
        {
            spdlog::error("[lua] callback error: {}", e.what());
        }
    }
}

bool callback_manager::invoke_consuming()
{
    std::lock_guard lk{impl_->mutex};

    if (impl_->consuming_slot)
    {
        try
        {
            if (impl_->consuming_slot()) return true;
        }
        catch (const std::exception& e)
        {
            spdlog::error("[lua] consuming callback error: {}", e.what());
        }
    }

    return false;
}

void callback_manager::clear()
{
    std::lock_guard lk{impl_->mutex};
    impl_->slots.clear();
    impl_->consuming_slot = nullptr;
}

bool callback_manager::empty() const
{
    std::lock_guard lk{impl_->mutex};
    return impl_->slots.empty();
}

} // namespace sdk::lua
