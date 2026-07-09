#include "sdk/graphics/rendering.hpp"

#include "sdk/core/contract.hpp"
#include "sdk/graphics/detail/camera_math.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <memory>
#include <mutex>
#include <ranges>
#include <utility>

namespace sdk::graphics
{

namespace
{

inline constexpr std::size_t k_maximum_line_vertices = 200'000;
inline constexpr double      k_minimum_pitch         = -89.0;
inline constexpr double      k_maximum_pitch         = 89.0;

std::atomic<render_api> g_backend{ render_api::unknown };
std::atomic<bool>       g_camera_enabled{ false };
std::mutex              g_camera_mutex;
camera_pose             g_camera_pose{
                .position      = { 0.0, 10.0, 0.0 },
                .yaw_degrees   = -90.0,
                .pitch_degrees = 0.0,
};
camera_pose                                     g_observed_camera;
bool                                            g_has_observed_camera{};
bool                                            g_adopted_live_camera{};
std::mutex                                      g_visual_mutex;
visual_settings                                 g_visual_settings;
std::atomic<std::uint64_t> g_line_generation{};
#if defined(__cpp_lib_atomic_shared_ptr) && \
    __cpp_lib_atomic_shared_ptr >= 201711L
std::atomic<std::shared_ptr<const detail::world_line_batch>> g_world_lines{
    std::make_shared<const detail::world_line_batch>()
};
#else
std::shared_ptr<const detail::world_line_batch> g_world_lines{
    std::make_shared<const detail::world_line_batch>()
};
#endif

void adopt_live_camera_locked() noexcept
{
    if (!g_has_observed_camera || g_adopted_live_camera)
    {
        return;
    }
    g_camera_pose         = g_observed_camera;
    g_adopted_live_camera = true;
}

[[nodiscard]] camera_pose sanitize_pose(camera_pose pose)
{
    require(detail::is_finite(pose), "camera pose must be finite");
    pose.pitch_degrees =
        std::clamp(pose.pitch_degrees, k_minimum_pitch, k_maximum_pitch);
    return pose;
}

void publish_world_lines(
    std::shared_ptr<const detail::world_line_batch> batch) noexcept
{
#if defined(__cpp_lib_atomic_shared_ptr) && \
    __cpp_lib_atomic_shared_ptr >= 201711L
    g_world_lines.store(std::move(batch), std::memory_order_release);
#else
    std::atomic_store_explicit(
        &g_world_lines, std::move(batch), std::memory_order_release);
#endif
}

[[nodiscard]] std::uint64_t next_line_generation() noexcept
{
    return g_line_generation.fetch_add(1, std::memory_order_relaxed) + 1U;
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
    const auto was_enabled =
        g_camera_enabled.exchange(enabled, std::memory_order::acq_rel);
    std::lock_guard lock{ g_camera_mutex };
    if (!enabled)
    {
        g_adopted_live_camera = false;
        return;
    }
    if (!was_enabled)
    {
        adopt_live_camera_locked();
    }
}

bool camera_enabled() noexcept
{
    return g_camera_enabled.load(std::memory_order::acquire);
}

void set_camera_pose(camera_pose pose)
{
    pose = sanitize_pose(pose);
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

void move_camera_local(double forward, double right, double up)
{
    require(std::isfinite(forward) && std::isfinite(right) && std::isfinite(up),
            "camera move: deltas must be finite");

    std::lock_guard lock{ g_camera_mutex };
    const auto      basis  = detail::basis_from_pose(g_camera_pose);
    g_camera_pose.position = detail::add(
        g_camera_pose.position,
        detail::add(detail::scale(basis.forward, forward),
                    detail::add(detail::scale(basis.right, right),
                                detail::scale(detail::world_up, up))));
}

void rotate_camera(double yaw_delta_degrees, double pitch_delta_degrees)
{
    require(std::isfinite(yaw_delta_degrees) &&
                std::isfinite(pitch_delta_degrees),
            "camera rotate: deltas must be finite");

    std::lock_guard lock{ g_camera_mutex };
    g_camera_pose.yaw_degrees =
        std::remainder(g_camera_pose.yaw_degrees + yaw_delta_degrees, 360.0);
    g_camera_pose.pitch_degrees =
        std::clamp(g_camera_pose.pitch_degrees + pitch_delta_degrees,
                   k_minimum_pitch,
                   k_maximum_pitch);
}

void set_world_lines(std::span<const line_vertex> vertices,
                     line_settings                settings)
{
    require((vertices.size() % 2U) == 0U,
            "set_world_lines: expected line-segment pairs");
    require(vertices.size() <= k_maximum_line_vertices,
            "set_world_lines: too many vertices");
    require(std::ranges::all_of(vertices,
                                [](const line_vertex& vertex)
                                {
                                    return std::isfinite(vertex.x) &&
                                           std::isfinite(vertex.y) &&
                                           std::isfinite(vertex.z);
                                }),
            "set_world_lines: vertices must be finite");

    auto batch        = std::make_shared<detail::world_line_batch>();
    batch->vertices   = { vertices.begin(), vertices.end() };
    batch->settings   = settings;
    batch->generation = next_line_generation();
    publish_world_lines(std::move(batch));
}

void clear_world_lines() noexcept
{
    auto batch        = std::make_shared<detail::world_line_batch>();
    batch->generation = next_line_generation();
    publish_world_lines(std::move(batch));
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

void observe_camera(camera_pose pose)
{
    pose = sanitize_pose(pose);
    std::lock_guard lock{ g_camera_mutex };
    g_observed_camera     = pose;
    g_has_observed_camera = true;
    // If freecam is already on, snap once to the live game camera so D3D8
    // does not keep the plugin default (0,10,0) after the first view lands.
    if (g_camera_enabled.load(std::memory_order::acquire))
    {
        adopt_live_camera_locked();
    }
}

auto world_lines_snapshot() noexcept -> std::shared_ptr<const world_line_batch>
{
#if defined(__cpp_lib_atomic_shared_ptr) && \
    __cpp_lib_atomic_shared_ptr >= 201711L
    return g_world_lines.load(std::memory_order_acquire);
#else
    return std::atomic_load_explicit(&g_world_lines, std::memory_order_acquire);
#endif
}

} // namespace detail

} // namespace sdk::graphics
