#include "scripting_backend.hpp"
#include "sdk.hpp"

#include <sol/sol.hpp>

#include <algorithm>
#include <filesystem>
#include <ranges>
#include <spdlog/spdlog.h>

// Forward declarations from bindings_*.cpp
namespace sdk::lua {
void register_sdk_bindings(sol::state& lua);
void register_ui_bindings(sol::state& lua);
void register_math_bindings(sol::state& lua);
void register_constant_bindings(sol::state& lua);
}

namespace sdk {

namespace {

// Wrap sol2 function into std::function with error checking
template <typename Sig>
auto wrap_lua_function(sol::protected_function fn) -> std::function<Sig>;

// Specialization for void()
template <>
auto wrap_lua_function<void()>(sol::protected_function fn) -> std::function<void()> {
    return [fn = std::move(fn)]() {
        auto result = fn();
        if (!result.valid()) {
            sol::error err = result;
            spdlog::error("[sdk] Lua callback error: {}", err.what());
        }
    };
}

// Specialization for bool(int)
template <>
auto wrap_lua_function<bool(int)>(sol::protected_function fn) -> std::function<bool(int)> {
    return [fn = std::move(fn)](int vk) -> bool {
        auto result = fn(vk);
        if (!result.valid()) {
            sol::error err = result;
            spdlog::error("[sdk] Lua callback error: {}", err.what());
            return false;
        }
        return result.get_type() == sol::type::boolean && result.get<bool>();
    };
}

// Specialization for bool(double×9)
template <>
auto wrap_lua_function<bool(double, double, double, double, double, double, double, double, double)>(
    sol::protected_function fn)
    -> std::function<bool(double, double, double, double, double, double, double, double, double)> {
    return [fn = std::move(fn)](double ex, double ey, double ez, double cx, double cy, double cz,
                                double ux, double uy, double uz) -> bool {
        auto result = fn(ex, ey, ez, cx, cy, cz, ux, uy, uz);
        if (!result.valid()) {
            sol::error err = result;
            spdlog::error("[sdk] Lua callback error: {}", err.what());
            return false;
        }
        return result.get_type() == sol::type::boolean && result.get<bool>();
    };
}

} // anonymous namespace

// sol2 backend implementation
class sol2_backend : public scripting_backend {
    std::unique_ptr<sol::state> lua;

public:
    void initialize() override {
        lua = std::make_unique<sol::state>();
        lua->open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table,
                            sol::lib::io, sol::lib::os, sol::lib::package);

        // Register callback registration functions
        sol::table sdk_table = lua->create_named_table("sdk");
        
        sdk_table.set_function("on_frame", [this](sol::protected_function fn) {
            sdk::on_frame(wrap_lua_function<void()>(std::move(fn)));
        });
        
        sdk_table.set_function("on_overlay", [this](sol::protected_function fn) {
            sdk::on_overlay(wrap_lua_function<void()>(std::move(fn)));
        });
        
        sdk_table.set_function("on_gl_identity", [this](sol::protected_function fn) {
            sdk::on_gl_identity(wrap_lua_function<void()>(std::move(fn)));
        });
        
        sdk_table.set_function("on_glu_lookat", [this](sol::protected_function fn) {
            sdk::on_glu_lookat(wrap_lua_function<bool(double, double, double, double, double,
                                                       double, double, double, double)>(std::move(fn)));
        });
        
        sdk_table.set_function("on_key_down", [this](sol::protected_function fn) {
            sdk::on_key_down(wrap_lua_function<bool(int)>(std::move(fn)));
        });
        
        sdk_table.set_function("on_load", [this](sol::protected_function fn) {
            sdk::on_load(wrap_lua_function<void()>(std::move(fn)));
        });
        
        sdk_table.set_function("on_unload", [this](sol::protected_function fn) {
            sdk::on_unload(wrap_lua_function<void()>(std::move(fn)));
        });

        spdlog::info("[sdk] sol2 backend initialized");
    }

    void register_bindings() override {
        if (!lua) {
            spdlog::error("[sdk] Cannot register bindings: Lua state not initialized");
            return;
        }

        lua::register_sdk_bindings(*lua);
        lua::register_ui_bindings(*lua);
        lua::register_math_bindings(*lua);
        lua::register_constant_bindings(*lua);

        spdlog::info("[sdk] Registered all bindings");
    }

    void load_plugins(const std::filesystem::path& directory) override {
        if (!lua) {
            spdlog::error("[sdk] Cannot load plugins: Lua state not initialized");
            return;
        }

        if (!std::filesystem::exists(directory)) {
            spdlog::warn("[sdk] Plugin directory does not exist: {}", directory.string());
            std::filesystem::create_directories(directory);
            return;
        }

        // Collect all .lua files
        std::vector<std::filesystem::path> lua_files;
        for (const auto& entry : std::filesystem::directory_iterator(directory)) {
            if (entry.is_regular_file() && entry.path().extension() == ".lua") {
                lua_files.push_back(entry.path());
            }
        }

        // Sort alphabetically for deterministic load order
        std::ranges::sort(lua_files);

        // Load each plugin
        for (const auto& path : lua_files) {
            spdlog::info("[sdk] Loading plugin: {}", path.filename().string());
            
            auto result = lua->safe_script_file(path.string(), sol::script_pass_on_error);
            if (!result.valid()) {
                sol::error err = result;
                spdlog::error("[sdk] Failed to load {}: {}", path.filename().string(), err.what());
            }
        }

        spdlog::info("[sdk] Loaded {} plugins", lua_files.size());
    }

    void shutdown() override {
        if (lua) {
            lua.reset();
            spdlog::info("[sdk] sol2 backend shutdown");
        }
    }

    ~sol2_backend() override {
        shutdown();
    }
};

// Factory function to create sol2 backend
std::unique_ptr<scripting_backend> create_sol2_backend() {
    return std::make_unique<sol2_backend>();
}

} // namespace sdk
