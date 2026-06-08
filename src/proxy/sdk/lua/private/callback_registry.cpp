/// @file callback_registry.cpp
/// @brief Implementation of the private callback registry.

#include "callback_registry.hpp"

#include <spdlog/spdlog.h>

namespace sdk::lua::detail
{

void callback_registry::add_lua_callback(const std::string_view event_name,
                                          sol::protected_function callback)
{
    std::lock_guard lock{mutex_};
    events_[std::string{event_name}].lua_callbacks.push_back(std::move(callback));
}

void callback_registry::add_cpp_callback(const std::string_view event_name,
                                          cpp_callback callback)
{
    std::lock_guard lock{mutex_};
    events_[std::string{event_name}].cpp_callbacks.push_back(std::move(callback));
}

std::size_t callback_registry::invoke(const std::string_view event_name)
{
    std::lock_guard lock{mutex_};

    const auto it{events_.find(std::string{event_name})};
    if (it == events_.end())
    {
        return 0;
    }

    std::size_t invoked{0};
    auto& [lua_cbs, cpp_cbs]{it->second};

    // Invoke Lua callbacks
    for (auto& fn : lua_cbs)
    {
        try
        {
            auto result{fn()};
            if (result.valid())
            {
                ++invoked;
            }
            else
            {
                log_lua_error(result);
            }
        }
        catch (const sol::error& e)
        {
            spdlog::error("[lua] callback '{}' threw: {}", event_name, e.what());
        }
    }

    // Invoke C++ callbacks
    for (auto& fn : cpp_cbs)
    {
        try
        {
            fn();
            ++invoked;
        }
        catch (const std::exception& e)
        {
            spdlog::error("[lua] C++ callback '{}' threw: {}", event_name, e.what());
        }
    }

    return invoked;
}

bool callback_registry::invoke_consuming(const std::string_view event_name)
{
    std::lock_guard lock{mutex_};

    const auto it{events_.find(std::string{event_name})};
    if (it == events_.end())
    {
        return false;
    }

    auto& [lua_cbs, cpp_cbs]{it->second};

    // Check Lua callbacks first
    for (auto& fn : lua_cbs)
    {
        try
        {
            auto result{fn()};
            if (!result.valid())
            {
                log_lua_error(result);
                continue;
            }

            // Check if callback returned true (consumed event)
            if (sol::optional<bool> consumed{result}; consumed.value_or(false))
            {
                return true;
            }
        }
        catch (const sol::error& e)
        {
            spdlog::error("[lua] callback '{}' threw: {}", event_name, e.what());
        }
    }

    // Check C++ callbacks (they don't consume events in this implementation)
    // If you need C++ callbacks to consume events, change their signature to return bool

    return false;
}

void callback_registry::clear(const std::string_view event_name)
{
    std::lock_guard lock{mutex_};
    events_.erase(std::string{event_name});
}

void callback_registry::clear_all()
{
    std::lock_guard lock{mutex_};
    events_.clear();
}

bool callback_registry::has_callbacks(const std::string_view event_name) const
{
    std::lock_guard lock{mutex_};

    const auto it{events_.find(std::string{event_name})};
    if (it == events_.end())
    {
        return false;
    }

    const auto& [lua_cbs, cpp_cbs]{it->second};
    return !lua_cbs.empty() || !cpp_cbs.empty();
}

std::size_t callback_registry::count(const std::string_view event_name) const
{
    std::lock_guard lock{mutex_};

    const auto it{events_.find(std::string{event_name})};
    if (it == events_.end())
    {
        return 0;
    }

    const auto& [lua_cbs, cpp_cbs]{it->second};
    return lua_cbs.size() + cpp_cbs.size();
}

void callback_registry::log_lua_error(const sol::protected_function_result& result)
{
    if (!result.valid())
    {
        sol::error err{result};
        spdlog::error("[lua] callback error: {}", err.what());
    }
}

} // namespace sdk::lua::detail
