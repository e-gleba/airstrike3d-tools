#pragma once

#include "renderer_interface.hpp"

#include <glm/glm.hpp>

#include <string>

namespace as3
{

// Unit type determines movement behavior
enum class unit_type : uint8_t
{
    static_object,  // Does not move
    ground_vehicle, // Moves on ground, rotates in place
    helicopter,     // Hovers, can move in any direction
    aircraft,       // Fast, wide turning radius
};

// Movement state machine
enum class move_state : uint8_t
{
    idle,     // Standing still
    rotating, // Turning to face target
    moving,   // Moving toward target
    hovering, // Helicopters only - hovering in place with bob
};

// Unit movement component
struct unit_movement
{
    unit_type type = unit_type::static_object;

    // Movement parameters
    float move_speed      = 10.0f; // Units per second
    float rotation_speed  = 90.0f; // Degrees per second
    float acceleration    = 5.0f;  // Speed change per second
    float hover_amplitude = 0.3f;  // Helicopter bob amount
    float hover_frequency = 1.5f;  // Helicopter bob speed
    float mass            = 1.0f;  // Affects acceleration/inertia

    // Current state
    move_state state           = move_state::idle;
    glm::vec3  target_position = glm::vec3(0.0f);
    float      current_speed   = 0.0f;
    float      hover_phase     = 0.0f;
    bool       has_target      = false;

    // Ground height (for vehicles)
    float ground_height = 0.0f;

    // For helicopters - flight height
    float flight_height = 5.0f;
};

// Unit data for scene objects
struct unit_data
{
    std::string name;
    std::string model_path;
    std::string texture_path;

    // Transform
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 rotation = glm::vec3(0.0f); // Euler degrees
    glm::vec3 scale    = glm::vec3(1.0f);

    // Movement
    unit_movement movement;

    // Selection
    bool selected = false;

    // Runtime
    model_handle model = invalid_model;
};

// Movement strategy functions
namespace movement
{
// Update unit movement based on type and state
void update(unit_data& unit, float dt);

// Command unit to move to position
void move_to(unit_data& unit, const glm::vec3& target);

// Stop unit movement
void stop(unit_data& unit);

// Check if unit can move
bool can_move(const unit_data& unit);

// Get angle between current facing and target direction
float angle_to_target(const unit_data& unit);

// Normalize angle to -180..180
float normalize_angle(float angle);
} // namespace movement

} // namespace as3
