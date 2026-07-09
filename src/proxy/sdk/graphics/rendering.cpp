#include "sdk/graphics/rendering.hpp"

#include <algorithm>
#include <atomic>
#include <mutex>
#include <utility>
#include <vector>

namespace sdk::graphics
{

namespace
{

std::atomic<render_api>  g_backend{ render_api::unknown };
std::atomic<bool>        g_camera_enabled{ false };
std::mutex               g_state_mutex;
camera_pose              g_camera_pose;
std::vector<line_vertex> g_world_lines;
visual_settings          g_visual_settings;

} // namespace

render_api active_backend() noexcept
{
    return g_backend.load(std::memory_order::acquire);
}

bool supports(capability value) noexcept
{
    const auto backend = active_backend();
    if (backend == render_api::unknown)
    {
        return false;
    }

    switch (value)
    {
        case capability::overlay:
        case capability::free_camera:
        case capability::world_lines:
        case capability::visual_override:
            return backend == render_api::opengl ||
                   backend == render_api::direct3d8;
    }

    return false;
}

void set_camera_enabled(bool enabled) noexcept
{
    g_camera_enabled.store(enabled, std::memory_order::release);
}

bool camera_enabled() noexcept
{
    return g_camera_enabled.load(std::memory_order::acquire);
}

void set_camera_pose(camera_pose pose) noexcept
{
    pose.pitch_degrees = std::clamp(pose.pitch_degrees, -89.0, 89.0);
    std::lock_guard lock{ g_state_mutex };
    g_camera_pose = pose;
}

camera_pose get_camera_pose() noexcept
{
    std::lock_guard lock{ g_state_mutex };
    return g_camera_pose;
}

void set_world_lines(std::span<const line_vertex> vertices)
{
    std::lock_guard lock{ g_state_mutex };
    g_world_lines.assign(vertices.begin(), vertices.end());
}

void clear_world_lines() noexcept
{
    std::lock_guard lock{ g_state_mutex };
    g_world_lines.clear();
}

void set_visual_settings(visual_settings settings) noexcept
{
    settings.alpha = std::clamp(settings.alpha, 0.0F, 1.0F);
    std::lock_guard lock{ g_state_mutex };
    g_visual_settings = settings;
}

visual_settings get_visual_settings() noexcept
{
    std::lock_guard lock{ g_state_mutex };
    return g_visual_settings;
}

namespace detail
{

void set_active_backend(render_api backend) noexcept
{
    g_backend.store(backend, std::memory_order::release);
}

auto world_lines_snapshot() -> std::vector<line_vertex>
{
    std::lock_guard lock{ g_state_mutex };
    return g_world_lines;
}

} // namespace detail

} // namespace sdk::graphics
