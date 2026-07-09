#include "sdk/graphics/rendering.hpp"

#include "sdk/graphics/detail/camera_math.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <memory>
#include <mutex>
#include <ranges>

namespace sdk::graphics
{

namespace
{

inline constexpr std::size_t k_maximum_line_vertices = 200'000;

std::atomic<render_api>    g_backend{ render_api::unknown };
std::atomic<bool>          g_camera_enabled{ false };
std::mutex                 g_camera_mutex;
camera_pose                g_camera_pose;
camera_pose                g_observed_camera;
bool                       g_has_observed_camera{};
std::mutex                 g_visual_mutex;
visual_settings            g_visual_settings;
std::atomic<std::uint64_t> g_line_generation{};
std::shared_ptr<const detail::world_line_batch> g_world_lines{
    std::make_shared<const detail::world_line_batch>()
};

[[nodiscard]] bool finite(camera_pose pose) noexcept
{
    return std::isfinite(pose.position.x) && std::isfinite(pose.position.y) &&
           std::isfinite(pose.position.z) && std::isfinite(pose.yaw_degrees) &&
           std::isfinite(pose.pitch_degrees);
}

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
    if (!finite(pose))
    {
        return;
    }
    pose.pitch_degrees = std::clamp(pose.pitch_degrees, -89.0, 89.0);
    std::lock_guard lock{ g_camera_mutex };
    g_camera_pose = pose;
}

camera_pose get_camera_pose() noexcept
{
    std::lock_guard lock{ g_camera_mutex };
    return g_camera_pose;
}

bool adopt_observed_camera() noexcept
{
    std::lock_guard lock{ g_camera_mutex };
    if (!g_has_observed_camera)
    {
        return false;
    }
    g_camera_pose = g_observed_camera;
    return true;
}

bool has_observed_camera() noexcept
{
    std::lock_guard lock{ g_camera_mutex };
    return g_has_observed_camera;
}

void move_camera_local(double forward, double right, double up) noexcept
{
    if (!std::isfinite(forward) || !std::isfinite(right) || !std::isfinite(up))
    {
        return;
    }

    std::lock_guard lock{ g_camera_mutex };
    const auto      basis  = detail::basis_from_pose(g_camera_pose);
    g_camera_pose.position = detail::add(
        g_camera_pose.position,
        detail::add(detail::scale(basis.forward, forward),
                    detail::add(detail::scale(basis.right, right),
                                detail::scale(detail::world_up, up))));
}

void rotate_camera(double yaw_delta_degrees,
                   double pitch_delta_degrees) noexcept
{
    if (!std::isfinite(yaw_delta_degrees) ||
        !std::isfinite(pitch_delta_degrees))
    {
        return;
    }

    std::lock_guard lock{ g_camera_mutex };
    g_camera_pose.yaw_degrees =
        std::remainder(g_camera_pose.yaw_degrees + yaw_delta_degrees, 360.0);
    g_camera_pose.pitch_degrees = std::clamp(
        g_camera_pose.pitch_degrees + pitch_delta_degrees, -89.0, 89.0);
}

bool set_world_lines(std::span<const line_vertex> vertices,
                     line_settings                settings)
{
    if ((vertices.size() % 2U) != 0U ||
        vertices.size() > k_maximum_line_vertices ||
        !std::ranges::all_of(vertices,
                             [](const line_vertex& vertex)
                             {
                                 return std::isfinite(vertex.x) &&
                                        std::isfinite(vertex.y) &&
                                        std::isfinite(vertex.z);
                             }))
    {
        return false;
    }

    auto batch      = std::make_shared<detail::world_line_batch>();
    batch->vertices = { vertices.begin(), vertices.end() };
    batch->settings = settings;
    batch->generation =
        g_line_generation.fetch_add(1, std::memory_order_relaxed) + 1U;
    std::atomic_store_explicit(
        &g_world_lines,
        std::shared_ptr<const detail::world_line_batch>{ std::move(batch) },
        std::memory_order_release);
    return true;
}

void clear_world_lines() noexcept
{
    auto batch = std::make_shared<detail::world_line_batch>();
    batch->generation =
        g_line_generation.fetch_add(1, std::memory_order_relaxed) + 1U;
    std::atomic_store_explicit(
        &g_world_lines,
        std::shared_ptr<const detail::world_line_batch>{ std::move(batch) },
        std::memory_order_release);
}

void set_visual_settings(visual_settings settings) noexcept
{
    settings.alpha = std::clamp(settings.alpha, 0.0F, 1.0F);
    std::lock_guard lock{ g_visual_mutex };
    g_visual_settings = settings;
}

visual_settings get_visual_settings() noexcept
{
    std::lock_guard lock{ g_visual_mutex };
    return g_visual_settings;
}

namespace detail
{

void set_active_backend(render_api backend) noexcept
{
    g_backend.store(backend, std::memory_order::release);
}

void observe_camera(camera_pose pose) noexcept
{
    if (!finite(pose))
    {
        return;
    }
    pose.pitch_degrees = std::clamp(pose.pitch_degrees, -89.0, 89.0);
    std::lock_guard lock{ g_camera_mutex };
    g_observed_camera     = pose;
    g_has_observed_camera = true;
}

auto world_lines_snapshot() noexcept -> std::shared_ptr<const world_line_batch>
{
    return std::atomic_load_explicit(&g_world_lines, std::memory_order_acquire);
}

} // namespace detail

} // namespace sdk::graphics
