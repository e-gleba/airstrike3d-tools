#include "unit.hpp"

#include <algorithm>
#include <cmath>

namespace as3::movement
{

float normalize_angle(float angle)
{
    while (angle > 180.0f)
        angle -= 360.0f;
    while (angle < -180.0f)
        angle += 360.0f;
    return angle;
}

float angle_to_target(const unit_data& unit)
{
    if (!unit.movement.has_target)
        return 0.0f;

    glm::vec3 dir = unit.movement.target_position - unit.position;
    dir.y         = 0.0f; // Ignore vertical

    if (glm::length(dir) < 0.01f)
        return 0.0f;

    dir = glm::normalize(dir);

    // Calculate target yaw (angle in XZ plane)
    float target_yaw = std::atan2(dir.x, dir.z) * (180.0f / 3.14159265f);

    // Models face -Z direction, so we add 180 for all movable units
    if (unit.movement.type == unit_type::helicopter ||
        unit.movement.type == unit_type::ground_vehicle ||
        unit.movement.type == unit_type::aircraft)
        target_yaw += 180.0f;

    // Current facing direction (yaw)
    float current_yaw = unit.rotation.y;

    return normalize_angle(target_yaw - current_yaw);
}

bool can_move(const unit_data& unit)
{
    return unit.movement.type != unit_type::static_object;
}

void move_to(unit_data& unit, const glm::vec3& target)
{
    if (!can_move(unit))
        return;

    unit.movement.has_target      = true;
    unit.movement.target_position = target;

    // Keep current height for helicopters
    if (unit.movement.type == unit_type::helicopter)
        unit.movement.target_position.y = unit.movement.flight_height;
    else
        unit.movement.target_position.y = unit.movement.ground_height;

    // Start by rotating toward target
    float angle_diff = std::abs(angle_to_target(unit));

    if (angle_diff > 5.0f)
        unit.movement.state = move_state::rotating;
    else
        unit.movement.state = move_state::moving;
}

void stop(unit_data& unit)
{
    unit.movement.has_target    = false;
    unit.movement.current_speed = 0.0f;

    if (unit.movement.type == unit_type::helicopter)
        unit.movement.state = move_state::hovering;
    else
        unit.movement.state = move_state::idle;
}

void update_ground_vehicle(unit_data& unit, float dt)
{
    auto& mv = unit.movement;

    switch (mv.state)
    {
        case move_state::idle:
            // Decelerate
            mv.current_speed =
                std::max(0.0f, mv.current_speed - mv.acceleration * dt);
            break;

        case move_state::rotating:
        {
            // Rotate toward target
            float angle_diff    = angle_to_target(unit);
            float rotate_amount = mv.rotation_speed * dt;

            if (std::abs(angle_diff) <= rotate_amount)
            {
                // Finished rotating
                unit.rotation.y += angle_diff;
                mv.state = move_state::moving;
            }
            else
            {
                // Continue rotating
                float sign = (angle_diff > 0) ? 1.0f : -1.0f;
                unit.rotation.y += sign * rotate_amount;
            }
            unit.rotation.y = normalize_angle(unit.rotation.y);

            // Slow down while rotating
            mv.current_speed =
                std::max(0.0f, mv.current_speed - mv.acceleration * dt);
            break;
        }

        case move_state::moving:
        {
            // Check if we need to adjust rotation
            float angle_diff = angle_to_target(unit);
            if (std::abs(angle_diff) > 15.0f)
            {
                mv.state = move_state::rotating;
                break;
            }

            // Smooth acceleration based on mass
            float accel = mv.acceleration / std::max(1.0f, mv.mass * 0.3f);
            mv.current_speed =
                std::min(mv.move_speed, mv.current_speed + accel * dt);

            // Move forward in facing direction (models face -Z, so negate)
            float     yaw_rad = unit.rotation.y * (3.14159265f / 180.0f);
            glm::vec3 forward(-std::sin(yaw_rad), 0.0f, -std::cos(yaw_rad));

            unit.position += forward * mv.current_speed * dt;
            unit.position.y = mv.ground_height;

            // Check if reached target
            glm::vec3 to_target = mv.target_position - unit.position;
            to_target.y         = 0.0f;
            float dist          = glm::length(to_target);

            if (dist < 1.0f)
            {
                unit.position.x = mv.target_position.x;
                unit.position.z = mv.target_position.z;
                stop(unit);
            }
            break;
        }

        default:
            break;
    }
}

void update_helicopter(unit_data& unit, float dt)
{
    constexpr float PI  = 3.14159265f;
    constexpr float TAU = 6.28318530f;

    auto& mv = unit.movement;

    // Update hover phase
    mv.hover_phase += dt * mv.hover_frequency * TAU;
    if (mv.hover_phase > TAU)
        mv.hover_phase -= TAU;

    // Gentle hover bob
    float hover_offset = std::sin(mv.hover_phase) * mv.hover_amplitude;

    // Target height
    float target_y = mv.flight_height + hover_offset;

    switch (mv.state)
    {
        case move_state::idle:
        case move_state::hovering:
        {
            // Smooth height interpolation when hovering
            unit.position.y +=
                (target_y - unit.position.y) * std::min(1.0f, dt * 3.0f);

            // Gentle sway
            float target_pitch = std::sin(mv.hover_phase * 0.4f) * 1.5f;
            float target_roll  = std::sin(mv.hover_phase * 0.3f) * 1.0f;

            unit.rotation.x +=
                (target_pitch - unit.rotation.x) * std::min(1.0f, dt * 2.0f);
            unit.rotation.z +=
                (target_roll - unit.rotation.z) * std::min(1.0f, dt * 2.0f);

            // Quick deceleration for helicopters
            mv.current_speed =
                std::max(0.0f, mv.current_speed - mv.acceleration * dt * 2.0f);
            break;
        }

        case move_state::rotating:
        {
            unit.position.y +=
                (target_y - unit.position.y) * std::min(1.0f, dt * 3.0f);

            float angle_diff    = angle_to_target(unit);
            float rotate_amount = mv.rotation_speed * dt;

            if (std::abs(angle_diff) <= rotate_amount)
            {
                unit.rotation.y += angle_diff;
                mv.state = move_state::moving;
            }
            else
            {
                float sign = (angle_diff > 0.0f) ? 1.0f : -1.0f;
                unit.rotation.y += sign * rotate_amount;

                // Bank while rotating (tilt into the turn)
                float target_roll = sign * 15.0f;
                unit.rotation.z +=
                    (target_roll - unit.rotation.z) * std::min(1.0f, dt * 5.0f);
            }
            unit.rotation.y = normalize_angle(unit.rotation.y);

            // Small forward tilt during rotation (negative = nose down)
            unit.rotation.x +=
                (-5.0f - unit.rotation.x) * std::min(1.0f, dt * 3.0f);
            break;
        }

        case move_state::moving:
        {
            // Maintain flight height
            unit.position.y +=
                (target_y - unit.position.y) * std::min(1.0f, dt * 4.0f);

            // Course correction while moving
            float angle_diff = angle_to_target(unit);
            if (std::abs(angle_diff) > 60.0f)
            {
                mv.state = move_state::rotating;
                break;
            }

            // Smooth rotation adjustment while flying
            if (std::abs(angle_diff) > 1.0f)
            {
                float sign = (angle_diff > 0.0f) ? 1.0f : -1.0f;
                unit.rotation.y += sign * mv.rotation_speed * 0.5f * dt;

                // Bank into turns
                float target_roll =
                    sign * std::min(20.0f, std::abs(angle_diff) * 0.5f);
                unit.rotation.z +=
                    (target_roll - unit.rotation.z) * std::min(1.0f, dt * 6.0f);
            }
            else
            {
                // Level out when flying straight
                unit.rotation.z +=
                    (0.0f - unit.rotation.z) * std::min(1.0f, dt * 4.0f);
            }

            // Fast acceleration for helicopters
            mv.current_speed = std::min(
                mv.move_speed, mv.current_speed + mv.acceleration * dt);

            // Move forward (models face -Z)
            float     yaw_rad = unit.rotation.y * (PI / 180.0f);
            glm::vec3 forward(-std::sin(yaw_rad), 0.0f, -std::cos(yaw_rad));
            unit.position += forward * mv.current_speed * dt;

            // Forward tilt based on speed (negative = nose down when flying)
            float speed_ratio = mv.current_speed / mv.move_speed;
            float target_tilt =
                -speed_ratio * 25.0f; // Negative for nose-down forward tilt
            unit.rotation.x +=
                (target_tilt - unit.rotation.x) * std::min(1.0f, dt * 4.0f);

            // Check if reached target
            glm::vec3 to_target = mv.target_position - unit.position;
            to_target.y         = 0.0f;

            if (glm::length(to_target) < 3.0f)
            {
                stop(unit);
            }
            break;
        }

        default:
            break;
    }
}

void update(unit_data& unit, float dt)
{
    switch (unit.movement.type)
    {
        case unit_type::ground_vehicle:
            update_ground_vehicle(unit, dt);
            break;

        case unit_type::helicopter:
            update_helicopter(unit, dt);
            break;

        case unit_type::aircraft:
            // TODO: Implement aircraft movement
            break;

        case unit_type::static_object:
        default:
            // No movement
            break;
    }
}

} // namespace as3::movement
