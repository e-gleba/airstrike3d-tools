#pragma once

// sdk/scripting/callback_registry.hpp — Type-erased callback management (pure C++23)
//
// Replaces callback_list that exposed sol::protected_function.
// Now uses std::function for type erasure.

#include <functional>
#include <mutex>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace sdk::scripting
{

// ─── Void callback list ──────────────────────────────────────────────────

class callback_list
{
public:
    explicit callback_list(std::recursive_mutex& mutex) noexcept
        : mutex_(mutex)
    {
    }

    void add(std::function<void()> fn)
    {
        std::lock_guard lock(mutex_);
        callbacks_.push_back(std::move(fn));
    }

    void invoke()
    {
        std::lock_guard lock(mutex_);
        for (auto& fn : callbacks_)
        {
            if (fn)
            {
                fn();
            }
        }
    }

    void clear()
    {
        std::lock_guard lock(mutex_);
        callbacks_.clear();
    }

    [[nodiscard]] auto empty() const noexcept -> bool
    {
        std::lock_guard lock(mutex_);
        return callbacks_.empty();
    }

private:
    std::recursive_mutex&         mutex_;
    std::vector<std::function<void()>> callbacks_;
};

// ─── Key callback list (consuming) ──────────────────────────────────────

class key_callback_list
{
public:
    explicit key_callback_list(std::recursive_mutex& mutex) noexcept
        : mutex_(mutex)
    {
    }

    void add(std::function<bool(int)> fn)
    {
        std::lock_guard lock(mutex_);
        callbacks_.push_back(std::move(fn));
    }

    [[nodiscard]] auto invoke_consuming(int key) -> bool
    {
        std::lock_guard lock(mutex_);
        for (auto& fn : callbacks_)
        {
            if (fn && fn(key))
            {
                return true; // Consumed
            }
        }
        return false;
    }

    void clear()
    {
        std::lock_guard lock(mutex_);
        callbacks_.clear();
    }

private:
    std::recursive_mutex&              mutex_;
    std::vector<std::function<bool(int)>> callbacks_;
};

// ─── Callback registry (manages all event types) ─────────────────────────

class callback_registry
{
public:
    callback_registry();
    ~callback_registry() = default;

    callback_registry(callback_registry const&)            = delete;
    auto operator=(callback_registry const&) -> callback_registry& = delete;
    callback_registry(callback_registry&&)                 = default;
    auto operator=(callback_registry&&) -> callback_registry& = default;

    // Access specific callback lists
    [[nodiscard]] auto on_frame() noexcept -> callback_list& { return on_frame_; }
    [[nodiscard]] auto on_overlay() noexcept -> callback_list& { return on_overlay_; }
    [[nodiscard]] auto on_gl_identity() noexcept -> callback_list& { return on_gl_identity_; }
    [[nodiscard]] auto on_glu_lookat() noexcept -> callback_list& { return on_glu_lookat_; }
    [[nodiscard]] auto on_load() noexcept -> callback_list& { return on_load_; }
    [[nodiscard]] auto on_unload() noexcept -> callback_list& { return on_unload_; }
    [[nodiscard]] auto on_key_down() noexcept -> key_callback_list& { return on_key_down_; }

    // Clear all callbacks
    void clear_all();

private:
    std::recursive_mutex mutex_;

    callback_list     on_frame_{mutex_};
    callback_list     on_overlay_{mutex_};
    callback_list     on_gl_identity_{mutex_};
    callback_list     on_glu_lookat_{mutex_};
    callback_list     on_load_{mutex_};
    callback_list     on_unload_{mutex_};
    key_callback_list on_key_down_{mutex_};
};

} // namespace sdk::scripting
