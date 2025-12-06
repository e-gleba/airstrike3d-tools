#include "scene.hpp"
#include "ui.hpp"

#include <imgui.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <mutex>

namespace
{

// Custom spdlog sink -> UI console
class ui_sink : public spdlog::sinks::base_sink<std::mutex>
{
protected:
    void sink_it_(const spdlog::details::log_msg& msg) override
    {
        int lvl = 2;
        switch (msg.level)
        {
            case spdlog::level::trace:    lvl = 0; break;
            case spdlog::level::debug:    lvl = 1; break;
            case spdlog::level::info:     lvl = 2; break;
            case spdlog::level::warn:     lvl = 3; break;
            case spdlog::level::err:
            case spdlog::level::critical: lvl = 4; break;
            default: break;
        }
        ui::log(lvl, std::string(msg.payload.data(), msg.payload.size()));
    }
    void flush_() override {}
};

std::shared_ptr<ui_sink> g_sink;

} // namespace

GAME_API euengine::preinit_result game_preinit(euengine::preinit_settings* s)
{
    s->window.title     = "euengine";
    s->window.width     = 1600;
    s->window.height    = 900;
    s->window.vsync     = euengine::vsync_mode::adaptive;
    s->window.resizable = true;
    s->window.high_dpi  = true;

    s->audio.master_volume = 0.8f;
    s->audio.music_volume  = 0.5f;

    s->background = { 0.12f, 0.14f, 0.18f, 1.0f };

    return euengine::preinit_result::ok;
}

GAME_API bool game_init(euengine::engine_context* ctx)
{
    // Add console sink
    g_sink = std::make_shared<ui_sink>();
    spdlog::default_logger()->sinks().push_back(g_sink);

    spdlog::info("Game module loaded");

    ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx->imgui_ctx));
    ui::init();

    scene::init(ctx);
    return true;
}

GAME_API void game_shutdown()
{
    spdlog::info("Game module unloaded");
    scene::shutdown();
    ui::log_clear();

    if (g_sink)
    {
        auto& sinks = spdlog::default_logger()->sinks();
        sinks.erase(std::remove(sinks.begin(), sinks.end(), g_sink), sinks.end());
        g_sink.reset();
    }
}

GAME_API void game_update(euengine::engine_context* ctx)
{
    scene::update(ctx);

    ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx->imgui_ctx));
    if (!ImGui::GetIO().WantCaptureMouse && ImGui::GetIO().MouseClicked[0])
        ctx->settings->set_mouse_captured(true);
}

GAME_API void game_render(euengine::engine_context* ctx)
{
    scene::render(ctx);
}

GAME_API void game_ui(euengine::engine_context* ctx)
{
    ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx->imgui_ctx));
    ui::draw(ctx);
}
