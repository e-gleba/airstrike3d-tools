#pragma once

#include "renderer_interface.hpp"

#include <glm/glm.hpp>
#include <yaml-cpp/yaml.h>

#include <filesystem>
#include <string>
#include <vector>

namespace as3
{

// Attachment point configuration
struct attachment_config
{
    std::string name;
    std::string model_path;
    std::string texture_path;  // Optional, empty = auto-detect
    glm::vec3   offset         = glm::vec3(0.0f);
    glm::vec3   rotation_axis  = glm::vec3(0.0f, 1.0f, 0.0f);
    float       rotation_speed = 0.0f;  // degrees/sec, 0 = static
    float       scale          = 1.0f;
};

// Runtime attachment state
struct attachment_state
{
    attachment_config config;
    model_handle      model    = invalid_model;
    float             rotation = 0.0f;  // Current rotation angle
};

// Composite model = body + attachments
struct composite_model_config
{
    std::string                    name;
    std::string                    body_path;
    std::string                    texture_path;
    std::vector<attachment_config> attachments;
};

class CompositeModel
{
public:
    CompositeModel() = default;
    ~CompositeModel() = default;

    // Load from config file or create empty
    bool load(IRenderer* renderer, const std::filesystem::path& config_path);
    bool load(IRenderer* renderer, const composite_model_config& config);
    void unload(IRenderer* renderer);

    // Update animations (call each frame)
    void update(float dt);

    // Draw at transform
    void draw(IRenderer* renderer, const transform& xform);

    // Accessors
    [[nodiscard]] bool is_loaded() const { return body_ != invalid_model; }
    [[nodiscard]] model_handle body() const { return body_; }
    [[nodiscard]] bounds get_bounds() const { return body_bounds_; }
    [[nodiscard]] const std::string& name() const { return config_.name; }

    // In-game editing
    [[nodiscard]] composite_model_config& config() { return config_; }
    [[nodiscard]] const composite_model_config& config() const { return config_; }
    [[nodiscard]] std::vector<attachment_state>& attachments() { return attachments_; }
    [[nodiscard]] const std::vector<attachment_state>& attachments() const { return attachments_; }

    // Save current configuration
    bool save(const std::filesystem::path& config_path) const;

    // Reload attachments after config change
    void reload_attachments(IRenderer* renderer);

private:
    composite_model_config        config_;
    model_handle                  body_        = invalid_model;
    bounds                        body_bounds_ = {};
    std::vector<attachment_state> attachments_;
};

// YAML serialization helpers
composite_model_config load_composite_config(const std::filesystem::path& path);
bool save_composite_config(const std::filesystem::path& path, const composite_model_config& config);

} // namespace as3

