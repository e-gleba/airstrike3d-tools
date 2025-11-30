#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <entt/entt.hpp>

namespace as3
{

struct CameraComponent
{
    glm::vec3 position    = glm::vec3(0.0f, 2.0f, 8.0f);
    glm::vec3 front       = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 up          = glm::vec3(0.0f, 1.0f, 0.0f);
    float     yaw         = -90.0f;
    float     pitch       = 0.0f;
    float     speed       = 5.0f;
    float     sensitivity = 0.1f;
    float     fov         = 75.0f;
    float     near_plane  = 0.1f;
    float     far_plane   = 100.0f;
    bool      active      = true;
};

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

    void update(entt::registry& registry, float delta_time, bool process_input);

    void handle_mouse_motion(entt::registry& registry,
                             float           xrel,
                             float           yrel);

    [[nodiscard]] CameraMatrices get_matrices(const entt::registry& registry,
                                              float aspect) const;

    [[nodiscard]] entt::entity get_active_camera(
        const entt::registry& registry) const;

private:
    void update_direction(CameraComponent& cam) const;
    void update_position(CameraComponent& cam, float delta_time) const;
};

} // namespace as3
