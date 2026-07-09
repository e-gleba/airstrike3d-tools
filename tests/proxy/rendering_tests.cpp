#include "sdk/graphics/detail/camera_math.hpp"
#include "sdk/graphics/rendering.hpp"

#include <array>
#include <cmath>
#include <cstdlib>

namespace
{

[[nodiscard]] bool near(double lhs, double rhs) noexcept
{
    return std::abs(lhs - rhs) < 0.000'001;
}

} // namespace

int main()
{
    using namespace sdk;
    using namespace sdk::graphics;

    if (active_backend() != render_api::unknown ||
        supports(capability::overlay))
    {
        return EXIT_FAILURE;
    }

    detail::set_active_backend(render_api::direct3d8);
    if (!supports(capability::overlay) || !supports(capability::free_camera) ||
        !supports(capability::world_lines) ||
        !supports(capability::visual_override))
    {
        return EXIT_FAILURE;
    }

    set_camera_enabled(true);
    set_camera_pose({
        .position      = { 1.0, 2.0, 3.0 },
        .yaw_degrees   = 45.0,
        .pitch_degrees = 120.0,
    });
    const auto pose = get_camera_pose();
    if (!camera_enabled() || !near(pose.position.x, 1.0) ||
        !near(pose.pitch_degrees, 89.0))
    {
        return EXIT_FAILURE;
    }

    constexpr auto lines = std::array{
        line_vertex{ .x = 1.0F, .argb = 0xFF112233U },
        line_vertex{ .x = 2.0F, .argb = 0xFF445566U },
    };
    if (!set_world_lines(lines))
    {
        return EXIT_FAILURE;
    }
    auto snapshot = detail::world_lines_snapshot();
    if (snapshot->vertices.size() != lines.size() ||
        snapshot->vertices[1].x != 2.0F)
    {
        return EXIT_FAILURE;
    }
    clear_world_lines();
    if (!detail::world_lines_snapshot()->vertices.empty())
    {
        return EXIT_FAILURE;
    }

    set_visual_settings({
        .mode  = visual_mode::ghost,
        .alpha = 2.0F,
    });
    const auto visual = get_visual_settings();
    if (visual.mode != visual_mode::ghost || visual.alpha != 1.0F)
    {
        return EXIT_FAILURE;
    }

    const camera_pose source{
        .position      = { 12.0, 34.0, -56.0 },
        .yaw_degrees   = -90.0,
        .pitch_degrees = 20.0,
    };
    const auto view           = detail::make_right_handed_view(source);
    const auto forward        = detail::basis_from_pose(source).forward;
    const auto view_forward_z = static_cast<double>(view[2]) * forward.x +
                                static_cast<double>(view[6]) * forward.y +
                                static_cast<double>(view[10]) * forward.z;
    if (!near(view_forward_z, -1.0))
    {
        return EXIT_FAILURE;
    }
    const auto round_trip = detail::decompose_right_handed_view(view);
    if (!round_trip || !near(round_trip->position.x, source.position.x) ||
        !near(round_trip->position.y, source.position.y) ||
        !near(round_trip->position.z, source.position.z) ||
        !near(round_trip->yaw_degrees, source.yaw_degrees) ||
        !near(round_trip->pitch_degrees, source.pitch_degrees))
    {
        return EXIT_FAILURE;
    }

    detail::observe_camera(source);
    if (!adopt_observed_camera())
    {
        return EXIT_FAILURE;
    }
    move_camera_local(10.0, 0.0, 0.0);
    const auto moved = get_camera_pose();
    if (!(moved.position.z < source.position.z))
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
