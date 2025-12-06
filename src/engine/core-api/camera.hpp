#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace euengine
{

struct camera_component final
{
    glm::vec3 position   = glm::vec3(0.0f, 5.0f, 10.0f);
    float     yaw        = -90.0f;
    float     pitch      = -20.0f;
    float     fov        = 60.0f;
    float     near_plane = 0.1f;
    float     far_plane  = 500.0f;
    float     move_speed = 10.0f;
    float     look_speed = 0.15f;

    [[nodiscard]] glm::vec3 front() const noexcept
    {
        return glm::normalize(glm::vec3 {
            std::cos(glm::radians(yaw)) * std::cos(glm::radians(pitch)),
            std::sin(glm::radians(pitch)),
            std::sin(glm::radians(yaw)) * std::cos(glm::radians(pitch)) });
    }

    [[nodiscard]] glm::vec3 right() const noexcept
    {
        return glm::normalize(
            glm::cross(front(), glm::vec3 { 0.0f, 1.0f, 0.0f }));
    }

    [[nodiscard]] glm::mat4 view() const noexcept
    {
        return glm::lookAt(
            position, position + front(), glm::vec3 { 0.0f, 1.0f, 0.0f });
    }

    [[nodiscard]] glm::mat4 projection(float aspect) const noexcept
    {
        return glm::perspective(
            glm::radians(fov), aspect, near_plane, far_plane);
    }
};

} // namespace euengine
