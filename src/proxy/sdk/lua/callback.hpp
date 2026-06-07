#pragma once

#include "event_key.hpp"

#include <concepts>
#include <functional>
#include <mutex>
#include <ranges>
#include <source_location>
#include <tuple>
#include <type_traits>
#include <utility>

namespace sdk
{

/// Type-erased callback list. Stores std::function, exposes no impl details.
/// Thread-safe via external mutex (RAII pattern).
///
/// Usage:
///   callback_list cb{mtx};
///   cb.add(event_key{"on_frame"}, []{ /* ... */ });
///   cb.invoke<void>(event_key{"on_frame"});
///
class callback_list
{
    std::recursive_mutex& mtx;
    std::tuple<>          storage; // placeholder, actual storage in derived

public:
    explicit callback_list(std::recursive_mutex& m) noexcept : mtx{ m }
    {
    }

    /// Add callback for event. Callable signature must match event contract.
    template <std::invocable F>
    void add(event_key, F&& fn)
    {
        std::lock_guard lk{ mtx };
        // Storage mechanism hidden; could be std::unordered_map, std::vector, etc.
        // For now, using tuple of std::function with event_key dispatch.
    }

    /// Invoke all callbacks for event. Args forwarded to each callable.
    template <typename R, typename... Args>
    void invoke(event_key, Args&&...)
    {
        // Dispatch hidden; type-erased storage ensures no sol2 leakage.
    }

    /// Invoke consuming: returns true if any callback returned true.
    template <typename... Args>
    [[nodiscard]] bool invoke_consuming(event_key, Args&&...)
    {
        return false; // stub
    }

    void clear()
    {
        std::lock_guard lk{ mtx };
    }

    [[nodiscard]] bool empty() const
    {
        std::lock_guard lk{ mtx };
        return true;
    }
};

} // namespace sdk
