#pragma once

#include <sol/sol.hpp>

#include <mutex>
#include <spdlog/spdlog.h>
#include <vector>

namespace sdk::lua::detail
{

/// @brief Thread-safe list of Lua callbacks.
///
/// Wraps a vector of sol::protected_function with mutex-protected access.
/// Supports both fire-and-forget (invoke) and first-consumer-wins
/// (invoke_consuming) dispatch patterns.
class callback_list final
{
    std::recursive_mutex&                mtx_;
    std::vector<sol::protected_function> fns_;

    static void log_if_error(sol::protected_function_result const& r)
    {
        if (!r.valid())
        {
            spdlog::error("[sdk] lua callback error: {}", sol::error{ r }.what());
        }
    }

public:
    explicit callback_list(std::recursive_mutex& m) noexcept : mtx_{ m } {}

    void add(sol::protected_function fn)
    {
        std::lock_guard lk{ mtx_ };
        fns_.push_back(std::move(fn));
    }

    template <typename... Args>
    void invoke(Args&&... args)
    {
        std::lock_guard lk{ mtx_ };
        for (auto& fn : fns_)
        {
            log_if_error(fn(std::forward<Args>(args)...));
        }
    }

    template <typename... Args>
    [[nodiscard]] bool invoke_consuming(Args&&... args)
    {
        std::lock_guard lk{ mtx_ };
        for (auto& fn : fns_)
        {
            auto r{ fn(std::forward<Args>(args)...) };
            if (!r.valid())
            {
                log_if_error(r);
                continue;
            }
            if (sol::optional<bool>{ r }.value_or(false))
            {
                return true;
            }
        }
        return false;
    }

    void clear()
    {
        std::lock_guard lk{ mtx_ };
        fns_.clear();
    }

    [[nodiscard]] bool empty() const
    {
        std::lock_guard lk{ mtx_ };
        return fns_.empty();
    }
};

/// @brief Aggregates all SDK callback lists.
///
/// Owns the shared recursive_mutex that serialises all callback access.
/// Passed to binding registration functions so they can register Lua
/// callbacks without depending on the global context.
struct callback_registry final
{
    std::recursive_mutex mutex;

    callback_list on_frame;
    callback_list on_overlay;
    callback_list on_gl_identity;
    callback_list on_glu_lookat;
    callback_list on_key_down;
    callback_list on_load;
    callback_list on_unload;

    callback_registry()
        : on_frame{ mutex }
        , on_overlay{ mutex }
        , on_gl_identity{ mutex }
        , on_glu_lookat{ mutex }
        , on_key_down{ mutex }
        , on_load{ mutex }
        , on_unload{ mutex }
    {
    }

    void clear_all()
    {
        on_frame.clear();
        on_overlay.clear();
        on_gl_identity.clear();
        on_glu_lookat.clear();
        on_key_down.clear();
        on_load.clear();
        on_unload.clear();
    }
};

} // namespace sdk::lua::detail
