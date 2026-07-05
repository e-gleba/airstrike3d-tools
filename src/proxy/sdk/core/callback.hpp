/// @file callback.hpp
/// @brief Type-erased, thread-safe callback lists for scripting ↔ C++ events.
///
/// Stores `std::function` — **no scripting backend types** leak through.
/// The conversion from backend-specific functions (e.g., `sol::protected_function`)
/// to `std::function` happens inside the scripting engine implementation.
///
/// Two list types:
///   - `callback_list<Args...>` — fire-and-forget, invokes all slots
///   - `consuming_callback_list<Args...>` — short-circuits on first `true`

#pragma once

#include <functional>
#include <mutex>
#include <ranges>
#include <utility>
#include <vector>

namespace sdk::callback
{

/// Thread-safe list of `void(Args...)` callbacks.
template <typename... Args>
class callback_list
{
public:
    using slot_fn = std::function<void(Args...)>;

    explicit callback_list(std::recursive_mutex& m) noexcept : mtx_{m} {}

    void add(slot_fn fn)
    {
        std::lock_guard lk{mtx_};
        fns_.push_back(std::move(fn));
    }

    void invoke(Args... args)
    {
        std::lock_guard lk{mtx_};
        for (auto& fn : fns_) { fn(args...); }
    }

    void clear()
    {
        std::lock_guard lk{mtx_};
        fns_.clear();
    }

    [[nodiscard]] bool empty() const
    {
        std::lock_guard lk{mtx_};
        return fns_.empty();
    }

    [[nodiscard]] std::size_t size() const
    {
        std::lock_guard lk{mtx_};
        return fns_.size();
    }

private:
    std::recursive_mutex& mtx_;
    std::vector<slot_fn>  fns_;
};

/// Thread-safe list of `bool(Args...)` callbacks with consuming semantics.
/// `invoke()` returns `true` as soon as any slot returns `true`.
template <typename... Args>
class consuming_callback_list
{
public:
    using slot_fn = std::function<bool(Args...)>;

    explicit consuming_callback_list(std::recursive_mutex& m) noexcept : mtx_{m} {}

    void add(slot_fn fn)
    {
        std::lock_guard lk{mtx_};
        fns_.push_back(std::move(fn));
    }

    [[nodiscard]] bool invoke(Args... args)
    {
        std::lock_guard lk{mtx_};
        // C++23: use ranges for cleaner iteration
        for (auto& fn : fns_ | std::views::all) {
            if (fn(args...)) return true;
        }
        return false;
    }

    void clear()
    {
        std::lock_guard lk{mtx_};
        fns_.clear();
    }

    [[nodiscard]] bool empty() const
    {
        std::lock_guard lk{mtx_};
        return fns_.empty();
    }

    [[nodiscard]] std::size_t size() const
    {
        std::lock_guard lk{mtx_};
        return fns_.size();
    }

private:
    std::recursive_mutex& mtx_;
    std::vector<slot_fn>  fns_;
};

} // namespace sdk::callback
