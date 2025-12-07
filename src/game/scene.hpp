#pragma once

#include <core-api/game.hpp>
#include <string>
#include <vector>

namespace scene
{

struct model_instance
{
    euengine::model_handle handle = euengine::invalid_model;
    std::string            name;
    std::string            path;
    euengine::transform    transform;
    euengine::bounds       bounds;
    bool                   animate     = false;
    float                  anim_speed  = 25.0f;
    bool                   hover       = false;
    float                  hover_base  = 0.0f;
    float                  hover_speed = 1.5f;
    float                  hover_range = 0.2f;
    bool                   moving      = false; // Moving along path
    float                  move_speed  = 0.5f;
    float                  move_path   = 0.0f; // Position along path (0-1)
    float     move_dir   = 1.0f; // Direction: 1.0 forward, -1.0 backward
    glm::vec3 move_start = {};
    glm::vec3 move_end   = {};
    glm::vec3 color_tint = { 1.0f, 1.0f, 1.0f }; // Color tint (white = no tint)
};

struct audio_file
{
    euengine::music_handle handle = euengine::invalid_music;
    std::string            name;
    std::string            path;
    bool                   is_sfx   = false;
    float                  duration = 0.0f;
};

// State
inline euengine::engine_context*          g_ctx = nullptr;
inline std::vector<euengine::mesh_handle> g_grids;
inline euengine::mesh_handle g_origin_axis = euengine::invalid_mesh;
inline bool                  g_show_origin = true;
inline entt::entity          g_camera      = entt::null;

inline std::vector<model_instance> g_models;
inline int                         g_selected = -1;

inline std::vector<audio_file> g_audio;
inline int                     g_playing = -1;

// Browser
inline std::vector<std::string> g_model_files;
inline int                      g_browser_sel = -1;
inline std::vector<std::string> g_scene_files;

// Info
inline std::string g_lib_path;
inline std::size_t g_lib_size = 0;

// Stats
inline int   g_draw_calls       = 0;
inline int   g_triangles        = 0;
inline float g_frame_times[300] = {}; // Extended for better history
inline float g_fps_history[300] = {}; // FPS tracking
inline int   g_frame_idx        = 0;
inline float g_min_fps          = 999.0f;
inline float g_max_fps          = 0.0f;
inline float g_avg_fps          = 0.0f;

void init(euengine::engine_context* ctx);
void shutdown();
void update(euengine::engine_context* ctx);
void render(euengine::engine_context* ctx);

void            scan_models();
void            scan_audio();
void            scan_scenes();
model_instance* add_model(const std::string& path,
                          const glm::vec3&   pos,
                          float              scale = 0.1f);
void            remove_model(int idx);
model_instance* duplicate_model(int idx);
void            focus_camera_on_object(int idx);
void            apply_sky();
void            rebuild_grid();

} // namespace scene
