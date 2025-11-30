#include "camera.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>

namespace as3
{

void CameraSystem::update_direction(CameraComponent& cam) const
{
    glm::vec3 direction{};
    direction.x =
        std::cos(glm::radians(cam.yaw)) * std::cos(glm::radians(cam.pitch));
    direction.y = std::sin(glm::radians(cam.pitch));
    direction.z =
        std::sin(glm::radians(cam.yaw)) * std::cos(glm::radians(cam.pitch));
    cam.front = glm::normalize(direction);
}

void CameraSystem::update_position(CameraComponent& cam, float delta_time) const
{
    const bool* keys     = SDL_GetKeyboardState(nullptr);
    const float velocity = cam.speed * delta_time;

    if (keys[SDL_SCANCODE_W])
        cam.position += cam.front * velocity;
    if (keys[SDL_SCANCODE_S])
        cam.position -= cam.front * velocity;

    const glm::vec3 right = glm::normalize(glm::cross(cam.front, cam.up));
    if (keys[SDL_SCANCODE_A])
        cam.position -= right * velocity;
    if (keys[SDL_SCANCODE_D])
        cam.position += right * velocity;

    if (keys[SDL_SCANCODE_SPACE])
        cam.position += cam.up * velocity;
    if (keys[SDL_SCANCODE_LCTRL])
        cam.position -= cam.up * velocity;
}

void CameraSystem::update(entt::registry& registry,
                          float           delta_time,
                          bool            process_input)
{
    auto view = registry.view<CameraComponent>();
    for (auto entity : view)
    {
        auto& cam = view.get<CameraComponent>(entity);
        if (cam.active && process_input)
        {
            update_position(cam, delta_time);
        }
    }
}

void CameraSystem::handle_mouse_motion(entt::registry& registry,
                                       float           xrel,
                                       float           yrel)
{
    auto view = registry.view<CameraComponent>();
    for (auto entity : view)
    {
        auto& cam = view.get<CameraComponent>(entity);
        if (cam.active)
        {
            cam.yaw += xrel * cam.sensitivity;
            cam.pitch =
                std::clamp(cam.pitch + (-yrel) * cam.sensitivity, k_min_pitch, k_max_pitch);
            update_direction(cam);
        }
    }
}

CameraMatrices CameraSystem::get_matrices(const entt::registry& registry,
                                          float                 aspect) const
{
    auto view = registry.view<const CameraComponent>();
    for (auto entity : view)
    {
        const auto& cam = view.get<const CameraComponent>(entity);
        if (cam.active)
        {
            CameraMatrices matrices{};
            matrices.view =
                glm::lookAt(cam.position, cam.position + cam.front, cam.up);
            matrices.projection = glm::perspective(
                glm::radians(cam.fov), aspect, cam.near_plane, cam.far_plane);
            matrices.view_projection = matrices.projection * matrices.view;
            return matrices;
        }
    }
    return {};
}

entt::entity CameraSystem::get_active_camera(
    const entt::registry& registry) const
{
    auto view = registry.view<const CameraComponent>();
    for (auto entity : view)
    {
        if (view.get<const CameraComponent>(entity).active)
        {
            return entity;
        }
    }
    return entt::null;
}

} // namespace as3
