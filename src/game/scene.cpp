#include "scene.hpp"

#include <spdlog/spdlog.h>
#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <fstream>

namespace YAML
{

template <> struct convert<glm::vec3>
{
    static Node encode(const glm::vec3& v)
    {
        Node node(NodeType::Sequence);
        node.push_back(v.x);
        node.push_back(v.y);
        node.push_back(v.z);
        return node;
    }

    static bool decode(const Node& node, glm::vec3& v)
    {
        if (!node.IsSequence() || node.size() != 3)
            return false;
        v.x = node[0].as<float>();
        v.y = node[1].as<float>();
        v.z = node[2].as<float>();
        return true;
    }
};

template <> struct convert<as3::unit_movement>
{
    static Node encode(const as3::unit_movement& mv)
    {
        Node node;
        node["type"]            = static_cast<int>(mv.type);
        node["move_speed"]      = mv.move_speed;
        node["rotation_speed"]  = mv.rotation_speed;
        node["acceleration"]    = mv.acceleration;
        node["hover_amplitude"] = mv.hover_amplitude;
        node["hover_frequency"] = mv.hover_frequency;
        node["flight_height"]   = mv.flight_height;
        node["mass"]            = mv.mass;
        return node;
    }

    static bool decode(const Node& node, as3::unit_movement& mv)
    {
        mv.type       = static_cast<as3::unit_type>(node["type"].as<int>(0));
        mv.move_speed = node["move_speed"].as<float>(10.0f);
        mv.rotation_speed  = node["rotation_speed"].as<float>(90.0f);
        mv.acceleration    = node["acceleration"].as<float>(5.0f);
        mv.hover_amplitude = node["hover_amplitude"].as<float>(0.3f);
        mv.hover_frequency = node["hover_frequency"].as<float>(1.5f);
        mv.flight_height   = node["flight_height"].as<float>(5.0f);
        mv.mass            = node["mass"].as<float>(1.0f);
        return true;
    }
};

template <> struct convert<as3::unit_data>
{
    static Node encode(const as3::unit_data& unit)
    {
        Node node;
        node["name"]  = unit.name;
        node["model"] = unit.model_path;
        if (!unit.texture_path.empty())
            node["texture"] = unit.texture_path;
        node["position"] = unit.position;
        node["rotation"] = unit.rotation;
        node["scale"]    = unit.scale;
        node["movement"] = unit.movement;
        return node;
    }

    static bool decode(const Node& node, as3::unit_data& unit)
    {
        unit.name         = node["name"].as<std::string>("Unit");
        unit.model_path   = node["model"].as<std::string>("");
        unit.texture_path = node["texture"].as<std::string>("");

        if (node["position"])
            unit.position = node["position"].as<glm::vec3>();
        if (node["rotation"])
            unit.rotation = node["rotation"].as<glm::vec3>();
        if (node["scale"])
            unit.scale = node["scale"].as<glm::vec3>();
        if (node["movement"])
            unit.movement = node["movement"].as<as3::unit_movement>();

        return true;
    }
};

template <> struct convert<as3::scene_config>
{
    static Node encode(const as3::scene_config& cfg)
    {
        Node node;
        node["name"]          = cfg.name;
        node["ambient_color"] = cfg.ambient_color;
        node["sun_direction"] = cfg.sun_direction;
        node["sun_color"]     = cfg.sun_color;

        Node units(NodeType::Sequence);
        for (const auto& unit : cfg.units)
            units.push_back(unit);
        node["units"] = units;

        return node;
    }

    static bool decode(const Node& node, as3::scene_config& cfg)
    {
        cfg.name = node["name"].as<std::string>("Untitled");

        if (node["ambient_color"])
            cfg.ambient_color = node["ambient_color"].as<glm::vec3>();
        if (node["sun_direction"])
            cfg.sun_direction = node["sun_direction"].as<glm::vec3>();
        if (node["sun_color"])
            cfg.sun_color = node["sun_color"].as<glm::vec3>();

        cfg.units.clear();
        if (auto units = node["units"])
        {
            for (const auto& u : units)
                cfg.units.push_back(u.as<as3::unit_data>());
        }

        return true;
    }
};

} // namespace YAML

namespace as3
{

void Scene::init(IRenderer* renderer)
{
    renderer_ = renderer;
    new_scene();
}

void Scene::shutdown()
{
    for (auto& unit : config_.units)
        unload_unit_model(unit);
    config_.units.clear();
    renderer_ = nullptr;
}

bool Scene::load(const std::filesystem::path& filepath)
{
    try
    {
        YAML::Node root = YAML::LoadFile(filepath.string());

        for (auto& unit : config_.units)
            unload_unit_model(unit);

        config_ = root.as<scene_config>();

        for (auto& unit : config_.units)
            load_unit_model(unit);

        current_path_   = filepath;
        dirty_          = false;
        selected_index_ = -1;

        spdlog::info(
            "Scene loaded: {} ({} units)", config_.name, config_.units.size());
        return true;
    }
    catch (const std::exception& e)
    {
        spdlog::error("Scene load error: {}", e.what());
        return false;
    }
}

bool Scene::save(const std::filesystem::path& filepath) const
{
    try
    {
        YAML::Node root = YAML::convert<scene_config>::encode(config_);

        std::ofstream file(filepath);
        if (!file)
        {
            spdlog::error("Cannot open file: {}", filepath.string());
            return false;
        }

        file << root;
        const_cast<Scene*>(this)->current_path_ = filepath;
        const_cast<Scene*>(this)->dirty_        = false;

        spdlog::info("Scene saved: {}", filepath.string());
        return true;
    }
    catch (const std::exception& e)
    {
        spdlog::error("Scene save error: {}", e.what());
        return false;
    }
}

bool Scene::save() const
{
    if (current_path_.empty())
        return false;
    return save(current_path_);
}

void Scene::new_scene(const std::string& name)
{
    for (auto& unit : config_.units)
        unload_unit_model(unit);

    config_      = scene_config{};
    config_.name = name;
    current_path_.clear();
    dirty_          = false;
    selected_index_ = -1;
}

void Scene::update(float dt)
{
    for (auto& unit : config_.units)
        movement::update(unit, dt);
}

void Scene::render()
{
    if (!renderer_)
        return;

    for (const auto& unit : config_.units)
    {
        if (unit.model == invalid_model)
            continue;

        renderer_->draw_model(unit.model,
                              { .position = unit.position,
                                .rotation = unit.rotation,
                                .scale    = unit.scale });
    }
}

unit_data& Scene::add_unit(const std::string& name, unit_type type)
{
    unit_data unit;
    unit.name          = name;
    unit.movement.type = type;

    // Default settings based on type
    switch (type)
    {
        case unit_type::helicopter:
            unit.movement.move_speed     = 15.0f;
            unit.movement.rotation_speed = 120.0f;
            unit.movement.flight_height  = 5.0f;
            unit.movement.state          = move_state::hovering;
            unit.position.y              = unit.movement.flight_height;
            break;
        case unit_type::ground_vehicle:
            unit.movement.move_speed     = 8.0f;
            unit.movement.rotation_speed = 60.0f;
            break;
        default:
            break;
    }

    config_.units.push_back(unit);
    dirty_ = true;
    return config_.units.back();
}

void Scene::remove_unit(size_t index)
{
    if (index >= config_.units.size())
        return;

    unload_unit_model(config_.units[index]);
    config_.units.erase(config_.units.begin() + static_cast<ptrdiff_t>(index));

    if (selected_index_ == static_cast<int>(index))
        selected_index_ = -1;
    else if (selected_index_ > static_cast<int>(index))
        selected_index_--;

    dirty_ = true;
}

void Scene::duplicate_unit(size_t index)
{
    if (index >= config_.units.size())
        return;

    unit_data copy = config_.units[index];
    copy.name += " (copy)";
    copy.position += glm::vec3(3.0f, 0.0f, 0.0f);
    copy.model               = invalid_model;
    copy.selected            = false;
    copy.movement.state      = move_state::idle;
    copy.movement.has_target = false;

    config_.units.push_back(copy);
    load_unit_model(config_.units.back());

    dirty_ = true;
}

void Scene::select_unit(size_t index)
{
    deselect_all();
    if (index < config_.units.size())
    {
        config_.units[index].selected = true;
        selected_index_               = static_cast<int>(index);
    }
}

void Scene::deselect_all()
{
    for (auto& unit : config_.units)
        unit.selected = false;
    selected_index_ = -1;
}

int Scene::get_selected_index() const
{
    return selected_index_;
}

unit_data* Scene::get_selected()
{
    if (selected_index_ >= 0 &&
        selected_index_ < static_cast<int>(config_.units.size()))
        return &config_.units[static_cast<size_t>(selected_index_)];
    return nullptr;
}

void Scene::command_move_to(const glm::vec3& target)
{
    auto* unit = get_selected();
    if (unit)
        movement::move_to(*unit, target);
}

void Scene::command_stop()
{
    auto* unit = get_selected();
    if (unit)
        movement::stop(*unit);
}

std::vector<std::string> Scene::list_scenes(const std::filesystem::path& dir)
{
    std::vector<std::string> scenes;

    try
    {
        if (!std::filesystem::exists(dir))
            return scenes;

        for (const auto& entry : std::filesystem::directory_iterator(dir))
        {
            if (entry.is_regular_file())
            {
                auto ext = entry.path().extension().string();
                if (ext == ".yaml" || ext == ".yml")
                    scenes.push_back(entry.path().stem().string());
            }
        }
        std::sort(scenes.begin(), scenes.end());
    }
    catch (const std::exception& e)
    {
        spdlog::error("Error listing scenes: {}", e.what());
    }

    return scenes;
}

void Scene::load_unit_model(unit_data& unit)
{
    if (!renderer_ || unit.model_path.empty())
        return;

    unit.model = renderer_->load_model(unit.model_path);
    if (unit.model == invalid_model)
        spdlog::warn("Failed to load model: {}", unit.model_path);
}

void Scene::unload_unit_model(unit_data& unit)
{
    if (!renderer_ || unit.model == invalid_model)
        return;

    renderer_->unload_model(unit.model);
    unit.model = invalid_model;
}

} // namespace as3
