#include "engine.hpp"

#include <sol/sol.hpp>

#include <filesystem>
#include <ranges>
#include <spdlog/spdlog.h>
#include <vector>

// Forward declarations — binding functions in bindings_*.cpp
namespace sdk::lua {
void register_sdk_bindings(sol::state& lua);
void register_ui_bindings(sol::state& lua);
void register_math_bindings(sol::state& lua);
void register_constant_bindings(sol::state& lua);
} // namespace sdk::lua

namespace sdk::lua {

struct engine::impl {
    sol::state lua;

    impl() {
        lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string,
                           sol::lib::table, sol::lib::io, sol::lib::os, sol::lib::package);
    }

    void register_bindings() {
        register_sdk_bindings(lua);
        register_ui_bindings(lua);
        register_math_bindings(lua);
        register_constant_bindings(lua);
    }

    void load_plugins(const std::filesystem::path& directory) {
        namespace fs = std::filesystem;

        if (!fs::exists(directory)) {
            spdlog::warn("[sdk] plugin directory '{}' not found, creating", directory.string());
            fs::create_directories(directory);
            return;
        }

        auto scripts = std::ranges::to<std::vector>(
            fs::directory_iterator(directory) |
            std::views::filter([](const auto& e) {
                return e.is_regular_file() && e.path().extension() == ".lua";
            }) |
            std::views::transform(&fs::directory_entry::path));

        std::ranges::sort(scripts);

        for (const auto& path : scripts) {
            spdlog::info("[sdk] loading plugin: {}", path.filename().string());

            auto result = lua.safe_script_file(path.string(), sol::script_pass_on_error);
            if (!result.valid()) {
                sol::error err = result;
                spdlog::error("[sdk] failed to load {}: {}", path.filename().string(), err.what());
            }
        }

        spdlog::info("[sdk] loaded {} plugins", scripts.size());
    }
};

// ─── Special members ──────────────────────────────────────────────────────────

engine::engine() = default;
engine::~engine() noexcept = default;
engine::engine(engine&&) noexcept = default;
engine& engine::operator=(engine&&) noexcept = default;

void engine::initialize() {
    pimpl_ = std::make_unique<impl>();
    spdlog::info("[sdk] Lua engine initialized");
}

void engine::register_bindings() {
    if (pimpl_) {
        pimpl_->register_bindings();
        spdlog::info("[sdk] all bindings registered");
    }
}

void engine::load_plugins(const std::filesystem::path& directory) {
    if (pimpl_) {
        pimpl_->load_plugins(directory);
    }
}

void engine::shutdown() {
    if (pimpl_) {
        pimpl_.reset();
        spdlog::info("[sdk] Lua engine shutdown");
    }
}

engine::operator bool() const noexcept {
    return pimpl_ != nullptr;
}

} // namespace sdk::lua
