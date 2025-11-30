#include "camera.hpp"

namespace as3
{

CameraMatrices CameraSystem::get_matrices(const entt::registry& registry,
                                          float                 aspect) const
{
    auto view = registry.view<const CameraComponent>();
    for (auto entity : view)
    {
        const auto& cam = view.get<const CameraComponent>(entity);

        CameraMatrices matrices{};
        matrices.view =
            glm::lookAt(cam.position, cam.position + cam.front, cam.up);
        matrices.projection = glm::perspective(
            glm::radians(cam.fov), aspect, cam.near_plane, cam.far_plane);
        matrices.view_projection = matrices.projection * matrices.view;
        return matrices;
    }
    return {};
}

entt::entity CameraSystem::get_active_camera(const entt::registry& registry) const
{
    auto view = registry.view<const CameraComponent>();
    for (auto entity : view)
    {
        return entity;
    }
    return entt::null;
}

} // namespace as3
