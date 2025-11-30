#pragma once

#include "camera.hpp" // shared camera component

#include <entt/entt.hpp>
#include <glm/glm.hpp>

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
    [[nodiscard]] CameraMatrices get_matrices(const entt::registry& registry, float aspect) const;
    [[nodiscard]] entt::entity get_active_camera(const entt::registry& registry) const;
};

} // namespace as3
