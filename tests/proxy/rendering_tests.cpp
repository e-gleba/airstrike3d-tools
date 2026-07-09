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
    set_world_lines(lines);
    auto snapshot = detail::world_lines_snapshot();
    if (snapshot.size() != lines.size() || snapshot[1].x != 2.0F)
    {
        return EXIT_FAILURE;
    }
    clear_world_lines();
    if (!detail::world_lines_snapshot().empty())
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

    return EXIT_SUCCESS;
}
