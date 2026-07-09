/// @file context.hpp
/// @brief Global application context — standard-library types only.

#pragma once

#include "sdk/core/types.hpp"
#include "sdk/scripting/callback.hpp"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string_view>
#include <utility>

namespace sdk
{

struct context final
{
    std::atomic<bool> imgui_initialized{ false };
    std::atomic<bool> should_unload{ false };
    std::atomic<bool> show_ui{ true };

    std::atomic<render_api> detected_api{ render_api::unknown };
    std::atomic<bool>       overlay_available{ false };
    std::atomic<matrix_mode> current_matrix_mode{ 0x1700 }; // GL_MODELVIEW

    std::recursive_mutex scripting_mutex;

    struct callbacks final
    {
        scripting::callback_list<> on_frame;
        scripting::callback_list<> on_overlay;
        scripting::callback_list<matrix_mode> on_gl_identity;
        scripting::consuming_callback_list<double, double, double,
                                           double, double, double,
                                           double, double, double> on_glu_lookat;
        scripting::consuming_callback_list<std::int32_t> on_key_down;
        scripting::callback_list<> on_load;
        scripting::callback_list<> on_unload;
    } cb;

    context()
        : cb{ .on_frame       = scripting::callback_list<>{ scripting_mutex },
              .on_overlay     = scripting::callback_list<>{ scripting_mutex },
              .on_gl_identity = scripting::callback_list<matrix_mode>{ scripting_mutex },
              .on_glu_lookat  = scripting::consuming_callback_list<
                  double, double, double, double, double, double,
                  double, double, double>{ scripting_mutex },
              .on_key_down    = scripting::consuming_callback_list<std::int32_t>{
                  scripting_mutex },
              .on_load        = scripting::callback_list<>{ scripting_mutex },
              .on_unload      = scripting::callback_list<>{ scripting_mutex } }
    {
    }

    template <typename... CBs>
    static void clear_all(CBs&... cbs)
    {
        (cbs.clear(), ...);
    }

    void clear_callbacks()
    {
        clear_all(cb.on_frame,
                  cb.on_overlay,
                  cb.on_gl_identity,
                  cb.on_glu_lookat,
                  cb.on_key_down,
                  cb.on_load,
                  cb.on_unload);
    }
};

inline context g_ctx;

constexpr std::int32_t k_ui_toggle_key = 0x2D; // VK_INSERT
constexpr std::string_view k_glsl_version{ "#version 110" };
/// Runtime plugin directory (deploy copies shared sources from repo `lua/`).
constexpr std::string_view k_plugin_dir{ "plugins" };

} // namespace sdk
