#include "composite_model.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <spdlog/spdlog.h>

#include <fstream>

namespace as3
{

namespace
{

glm::vec3 yaml_to_vec3(const YAML::Node& node, const glm::vec3& def = glm::vec3(0.0f))
{
    if (!node || !node.IsSequence() || node.size() < 3)
        return def;
    return {
        node[0].as<float>(def.x),
        node[1].as<float>(def.y),
        node[2].as<float>(def.z)
    };
}

YAML::Node vec3_to_yaml(const glm::vec3& v)
{
    YAML::Node node;
    node.push_back(v.x);
    node.push_back(v.y);
    node.push_back(v.z);
    return node;
}

} // namespace

bool CompositeModel::load(IRenderer* renderer, const std::filesystem::path& config_path)
{
    auto cfg = load_composite_config(config_path);
    if (cfg.body_path.empty())
    {
        spdlog::error("composite: failed to load config {}", config_path.string());
        return false;
    }
    return load(renderer, cfg);
}

bool CompositeModel::load(IRenderer* renderer, const composite_model_config& config)
{
    unload(renderer);
    config_ = config;

    // Load body
    body_ = renderer->load_model(config_.body_path);
    if (body_ == invalid_model)
    {
        spdlog::error("composite: failed to load body {}", config_.body_path);
        return false;
    }
    body_bounds_ = renderer->get_bounds(body_);

    // Load attachments
    for (const auto& att_cfg : config_.attachments)
    {
        attachment_state state;
        state.config   = att_cfg;
        state.rotation = 0.0f;
        state.model    = renderer->load_model(att_cfg.model_path);
        
        if (state.model == invalid_model)
            spdlog::warn("composite: failed to load attachment {}", att_cfg.model_path);
        
        attachments_.push_back(state);
    }

    spdlog::info("composite: loaded '{}' with {} attachments", 
                 config_.name.empty() ? config_.body_path : config_.name,
                 attachments_.size());
    return true;
}

void CompositeModel::unload(IRenderer* renderer)
{
    if (body_ != invalid_model)
    {
        renderer->unload_model(body_);
        body_ = invalid_model;
    }

    for (auto& att : attachments_)
    {
        if (att.model != invalid_model)
            renderer->unload_model(att.model);
    }
    attachments_.clear();
    config_ = {};
    body_bounds_ = {};
}

void CompositeModel::update(float dt)
{
    for (auto& att : attachments_)
    {
        if (att.config.rotation_speed != 0.0f)
        {
            att.rotation += att.config.rotation_speed * dt;
            if (att.rotation > 360.0f)
                att.rotation -= 360.0f;
            else if (att.rotation < 0.0f)
                att.rotation += 360.0f;
        }
    }
}

void CompositeModel::draw(IRenderer* renderer, const transform& xform)
{
    if (body_ == invalid_model)
        return;

    // Draw body
    renderer->draw_model(body_, xform);

    // Build parent transform matrix
    glm::mat4 parent = glm::mat4(1.0f);
    parent = glm::translate(parent, xform.position);
    parent = glm::rotate(parent, glm::radians(xform.rotation.x), glm::vec3(1, 0, 0));
    parent = glm::rotate(parent, glm::radians(xform.rotation.y), glm::vec3(0, 1, 0));
    parent = glm::rotate(parent, glm::radians(xform.rotation.z), glm::vec3(0, 0, 1));
    parent = glm::scale(parent, xform.scale);

    // Draw attachments
    for (const auto& att : attachments_)
    {
        if (att.model == invalid_model)
            continue;

        // Attachment position in parent space
        glm::vec4 local_pos = glm::vec4(att.config.offset, 1.0f);
        glm::vec3 world_pos = glm::vec3(parent * local_pos);

        // Calculate attachment rotation
        // First apply parent rotation, then local spin around axis
        glm::vec3 att_rotation = xform.rotation;
        
        // Add local rotation around the specified axis
        if (att.config.rotation_speed != 0.0f)
        {
            const auto& axis = att.config.rotation_axis;
            att_rotation.x += att.rotation * axis.x;
            att_rotation.y += att.rotation * axis.y;
            att_rotation.z += att.rotation * axis.z;
        }

        renderer->draw_model(att.model, {
            .position = world_pos,
            .rotation = att_rotation,
            .scale    = xform.scale * att.config.scale
        });
    }
}

bool CompositeModel::save(const std::filesystem::path& config_path) const
{
    return save_composite_config(config_path, config_);
}

void CompositeModel::reload_attachments(IRenderer* renderer)
{
    // Unload old attachments
    for (auto& att : attachments_)
    {
        if (att.model != invalid_model)
            renderer->unload_model(att.model);
    }
    attachments_.clear();

    // Reload from config
    for (const auto& att_cfg : config_.attachments)
    {
        attachment_state state;
        state.config   = att_cfg;
        state.rotation = 0.0f;
        state.model    = renderer->load_model(att_cfg.model_path);
        attachments_.push_back(state);
    }
}

composite_model_config load_composite_config(const std::filesystem::path& path)
{
    composite_model_config config;
    
    try
    {
        YAML::Node root = YAML::LoadFile(path.string());
        
        config.name         = root["name"].as<std::string>("");
        config.body_path    = root["body"].as<std::string>("");
        config.texture_path = root["texture"].as<std::string>("");

        if (auto attachments = root["attachments"])
        {
            for (const auto& att : attachments)
            {
                attachment_config ac;
                ac.name           = att["name"].as<std::string>("");
                ac.model_path     = att["model"].as<std::string>("");
                ac.texture_path   = att["texture"].as<std::string>("");
                ac.offset         = yaml_to_vec3(att["offset"]);
                ac.rotation_axis  = yaml_to_vec3(att["rotation_axis"], glm::vec3(0, 1, 0));
                ac.rotation_speed = att["rotation_speed"].as<float>(0.0f);
                ac.scale          = att["scale"].as<float>(1.0f);
                config.attachments.push_back(ac);
            }
        }
    }
    catch (const std::exception& e)
    {
        spdlog::error("composite config load error: {}", e.what());
        return {};
    }

    return config;
}

bool save_composite_config(const std::filesystem::path& path, const composite_model_config& config)
{
    try
    {
        YAML::Node root;
        root["name"]    = config.name;
        root["body"]    = config.body_path;
        root["texture"] = config.texture_path;

        YAML::Node attachments;
        for (const auto& att : config.attachments)
        {
            YAML::Node att_node;
            att_node["name"]           = att.name;
            att_node["model"]          = att.model_path;
            att_node["texture"]        = att.texture_path;
            att_node["offset"]         = vec3_to_yaml(att.offset);
            att_node["rotation_axis"]  = vec3_to_yaml(att.rotation_axis);
            att_node["rotation_speed"] = att.rotation_speed;
            att_node["scale"]          = att.scale;
            attachments.push_back(att_node);
        }
        root["attachments"] = attachments;

        std::ofstream out(path);
        if (!out)
        {
            spdlog::error("composite: cannot write to {}", path.string());
            return false;
        }
        out << root;
        spdlog::info("composite: saved config to {}", path.string());
        return true;
    }
    catch (const std::exception& e)
    {
        spdlog::error("composite config save error: {}", e.what());
        return false;
    }
}

} // namespace as3

