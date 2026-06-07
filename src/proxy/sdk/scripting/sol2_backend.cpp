// sdk/scripting/sol2_backend.cpp — sol2 implementation of scripting::state
//
// ALL sol2 code isolated here. Public header (state.hpp) remains pure C++23.
// This file implements the abstract state interface using sol2.

#include "sdk/scripting/state.hpp"

#include <sol/sol.hpp>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <ranges>
#include <unordered_map>

namespace sdk::scripting
{

// ─── sol2 state implementation ───────────────────────────────────────────

class sol2_state final : public state
{
public:
    sol2_state()
    {
        lua_.open_libraries(sol::lib::base,
                           sol::lib::math,
                           sol::lib::string,
                           sol::lib::table,
                           sol::lib::io,
                           sol::lib::os,
                           sol::lib::package);
    }

    ~sol2_state() override = default;

    auto execute_file(std::filesystem::path const& path) 
        -> std::expected<void, script_error> override
    {
        auto result = lua_.safe_script_file(path.string(), sol::script_pass_on_error);
        if (!result.valid())
        {
            sol::error err = result;
            return std::unexpected<script_error>{ { err.what(), path.string(), 0 } };
        }
        return {};
    }

    auto execute_string(std::string_view code) 
        -> std::expected<void, script_error> override
    {
        auto result = lua_.safe_script(std::string(code), sol::script_pass_on_error);
        if (!result.valid())
        {
            sol::error err = result;
            return std::unexpected<script_error>{ { err.what(), "<string>", 0 } };
        }
        return {};
    }

    void register_callback(std::string_view event_name, void_callback fn) override
    {
        std::string key(event_name);
        void_callbacks_[key].push_back(std::move(fn));
    }

    void register_key_callback(std::string_view event_name, int_callback fn) override
    {
        std::string key(event_name);
        key_callbacks_[key].push_back(std::move(fn));
    }

    void invoke_callbacks(std::string_view event_name) override
    {
        std::string key(event_name);
        if (auto it = void_callbacks_.find(key); it != void_callbacks_.end())
        {
            for (auto& fn : it->second)
            {
                if (fn)
                {
                    fn();
                }
            }
        }
    }

    auto invoke_key_callbacks(std::string_view event_name, int key) -> bool override
    {
        std::string event(event_name);
        if (auto it = key_callbacks_.find(event); it != key_callbacks_.end())
        {
            for (auto& fn : it->second)
            {
                if (fn && fn(key))
                {
                    return true;
                }
            }
        }
        return false;
    }

    void register_sdk_function(std::string_view name, std::function<void()> fn) override
    {
        // TODO: Implement SDK function binding to Lua
        // This requires wrapping std::function<void()> into sol::function
        spdlog::debug("[scripting] register_sdk_function: {} (stub)", name);
    }

    void register_math_function(std::string_view name, std::function<void()> fn) override
    {
        spdlog::debug("[scripting] register_math_function: {} (stub)", name);
    }

    void register_ui_function(std::string_view name, std::function<void()> fn) override
    {
        spdlog::debug("[scripting] register_ui_function: {} (stub)", name);
    }

    void register_int_constant(std::string_view table, std::string_view name, int value) override
    {
        if (!lua_[std::string(table)].valid())
        {
            lua_.create_named_table(std::string(table));
        }
        lua_[std::string(table)][std::string(name)] = value;
    }

    void register_float_constant(std::string_view table, std::string_view name, double value) override
    {
        if (!lua_[std::string(table)].valid())
        {
            lua_.create_named_table(std::string(table));
        }
        lua_[std::string(table)][std::string(name)] = value;
    }

    void clear_all_callbacks() override
    {
        void_callbacks_.clear();
        key_callbacks_.clear();
    }

private:
    sol::state lua_;
    std::unordered_map<std::string, std::vector<void_callback>> void_callbacks_;
    std::unordered_map<std::string, std::vector<int_callback>>  key_callbacks_;
};

// ─── Factory ──────────────────────────────────────────────────────────────

auto state::create() -> std::unique_ptr<state>
{
    return std::make_unique<sol2_state>();
}

} // namespace sdk::scripting
