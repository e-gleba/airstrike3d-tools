#pragma once

#include <core-api/game.hpp>
#include <deque>
#include <string>

namespace ui
{

// Console log
struct log_entry
{
    std::string message;
    int         level;
    float       time;
};

inline std::deque<log_entry> g_log;
inline constexpr std::size_t g_log_max    = 1000;
inline bool                  g_log_scroll = true;
inline float                 g_time       = 0.0f;
inline std::string           g_console_filter;
inline std::string           g_browser_filter;
inline std::string           g_audio_filter;
inline std::filesystem::path g_file_dialog_current_path =
    std::filesystem::current_path();
inline std::filesystem::path g_file_dialog_selected_file;
inline bool                  g_reset_window_layout = false;

// Panels
inline bool g_show_hierarchy   = true;
inline bool g_show_inspector   = true;
inline bool g_show_browser     = false;
inline bool g_show_audio       = false;
inline bool g_show_engine      = true;
inline bool g_show_console     = false;
inline bool g_show_stats       = true;
inline bool g_show_file_dialog = false;

// Scene
inline bool  g_wireframe     = false;
inline bool  g_auto_rotate   = true;
inline bool  g_grid_snap     = false;
inline float g_snap_size     = 1.0f;
inline float g_sky_color[3]  = { 0.12f, 0.14f, 0.18f };
inline float g_grid_color[3] = { 0.22f, 0.24f, 0.22f };

// Audio
inline float g_volume    = 50.0f;
inline float g_music_pos = 0.0f;

void log(int level, const std::string& msg);
void log_clear();

void init();
void draw_file_dialog();
void draw(euengine::engine_context* ctx);

} // namespace ui
