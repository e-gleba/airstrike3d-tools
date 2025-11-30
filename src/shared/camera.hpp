#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace as3
{

// Camera component - shared between engine and game
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
};

} // namespace as3

