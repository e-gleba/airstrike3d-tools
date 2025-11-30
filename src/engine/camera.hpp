#pragma once

#include "../shared/camera.hpp"

#include <entt/entt.hpp>

namespace as3
{

struct CameraMatrices
{
    glm::mat4 view;
    glm::mat4 projection;
    glm::mat4 view_projection;
};

class CameraSystem
{
public:
    static constexpr float k_max_pitch = 89.0f;
    static constexpr float k_min_pitch = -89.0f;

    [[nodiscard]] CameraMatrices get_matrices(const entt::registry& registry,
                                              float                 aspect) const;

    [[nodiscard]] entt::entity get_active_camera(
        const entt::registry& registry) const;
};

} // namespace as3
