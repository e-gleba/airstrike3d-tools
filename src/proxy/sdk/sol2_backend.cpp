#include "scripting_backend.hpp"
#include "sdk.hpp"

#include <sol/sol.hpp>

#include <filesystem>
#include <ranges>
#include <spdlog/spdlog.h>
#include <vector>

// Forward declarations — binding functions are internal, live in bindings_*.cpp
namespace sdk::lua {
void register_sdk_bindings(sol::state& lua);
void register_ui_bindings(sol::state& lua);
void register_math_bindings(sol::state& lua);
void register_constant_bindings(sol::state& lua);
} // namespace sdk::lua

namespace sdk {

class sol2_backend : public scripting_backend {
    std::unique_ptr<sol::state> lua;

public:
    void initialize() override {
        lua = std::make_unique<sol::state>();
        lua->open_libraries(sol::lib::base, sol::lib::math, sol::lib::string,
                            sol::lib::table, sol::lib::io, sol::lib::os,
                            sol::lib::package);
        spdlog::info("[sdk] sol2 backend initialized");
    }

    void register_bindings() override {
        if (!lua) return;

        lua::register_sdk_bindings(*lua);       // creates sdk table + callbacks + GL/input
        lua::register_ui_bindings(*lua);         // creates ui table
        lua::register_math_bindings(*lua);       // creates gmath table
        lua::register_constant_bindings(*lua);   // creates VK/GL tables

        spdlog::info("[sdk] all bindings registered");
    }

    void load_plugins(const std::filesystem::path& directory) override {
        if (!lua) return;

        if (!std::filesystem::exists(directory)) {
            spdlog::warn("[sdk] plugin directory '{}' not found, creating",
                         directory.string());
            std::filesystem::create_directories(directory);
            return;
        }

        auto scripts = std::ranges::to<std::vector>(
            std::filesystem::directory_iterator(directory) |
            std::views::filter([](const auto& e) {
                return e.is_regular_file() && e.path().extension() == ".lua";
            }) |
            std::views::transform(&std::filesystem::directory_entry::path));

        std::ranges::sort(scripts);

        for (const auto& path : scripts) {
            spdlog::info("[sdk] loading plugin: {}", path.filename().string());

            auto result = lua->safe_script_file(path.string(), sol::script_pass_on_error);
            if (!result.valid()) {
                sol::error err = result;
                spdlog::error("[sdk] failed to load {}: {}",
                              path.filename().string(), err.what());
            }
        }

        spdlog::info("[sdk] loaded {} plugins", scripts.size());
    }

    void shutdown() override {
        if (lua) {
            lua.reset();
            spdlog::info("[sdk] sol2 backend shutdown");
        }
    }

    ~sol2_backend() override { shutdown(); }
};

std::unique_ptr<scripting_backend> create_sol2_backend() {
    return std::make_unique<sol2_backend>();
}

} // namespace sdk
