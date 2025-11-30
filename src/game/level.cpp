#include "level.hpp"

#include <spdlog/spdlog.h>
#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <fstream>

namespace as3
{

bool Level::load(const std::filesystem::path& yaml_path)
{
    unload();

    try
    {
        YAML::Node root = YAML::LoadFile(yaml_path.string());

        // Header
        header_.name = root["name"].as<std::string>("");
        if (auto h = root["header"])
        {
            header_.grid_size       = h["grid_size"].as<int>(32);
            header_.terrain_scale   = h["terrain_scale"].as<int>(256);
            header_.object_count    = h["object_count"].as<int>(0);
        }
        header_.heightmap_layers = root["heightmap_layers"].as<int>(1);

        // Object types
        if (auto types = root["object_types"])
        {
            for (const auto& t : types)
                object_types_.push_back(t.as<std::string>());
        }

        // Item types
        if (auto types = root["item_types"])
        {
            for (const auto& t : types)
                item_types_.push_back(t.as<std::string>());
        }

        // Heightmaps - combine all layers into one
        if (auto heightmaps = root["heightmaps"])
        {
            for (const auto& layer : heightmaps)
            {
                for (const auto& row : layer)
                {
                    std::vector<int> row_data;
                    for (const auto& val : row)
                        row_data.push_back(val.as<int>(0));
                    combined_heightmap_.push_back(std::move(row_data));
                }
            }
        }

        // Objects
        if (auto objs = root["objects"])
        {
            for (const auto& obj : objs)
            {
                level_object lo;
                lo.type_name = obj["type_name"].as<std::string>("");
                if (auto pos = obj["position"])
                {
                    lo.position.x = pos["x"].as<float>(0.0f);
                    lo.position.y = pos["y"].as<float>(0.0f);
                    lo.position.z = pos["z"].as<float>(0.0f);
                }
                lo.rotation = obj["rotation"].as<float>(0.0f);
                lo.flags    = obj["flags"].as<int>(0);
                objects_.push_back(std::move(lo));
            }
        }

        loaded_ = true;
        spdlog::info("Level loaded: {} ({} objects, {}x{} terrain)",
                     header_.name, objects_.size(),
                     header_.grid_size, combined_heightmap_.size());
        return true;
    }
    catch (const std::exception& e)
    {
        spdlog::error("Level load error: {}", e.what());
        return false;
    }
}

void Level::unload()
{
    loaded_ = false;
    header_ = {};
    object_types_.clear();
    item_types_.clear();
    objects_.clear();
    combined_heightmap_.clear();
}

glm::vec3 Level::get_world_size() const
{
    if (!loaded_)
        return glm::vec3(0.0f);

    float scale = static_cast<float>(header_.terrain_scale) / static_cast<float>(header_.grid_size);
    return glm::vec3(
        static_cast<float>(header_.grid_size) * scale,
        static_cast<float>(combined_heightmap_.size()) * scale,
        255.0f * 0.1f  // Max height
    );
}

float Level::get_height_at(float x, float y) const
{
    if (!loaded_ || combined_heightmap_.empty())
        return 0.0f;

    float scale = static_cast<float>(header_.terrain_scale) / static_cast<float>(header_.grid_size);
    int gx = static_cast<int>(x / scale);
    int gy = static_cast<int>(y / scale);

    if (gx < 0 || gy < 0)
        return 0.0f;
    if (gy >= static_cast<int>(combined_heightmap_.size()))
        return 0.0f;
    if (gx >= static_cast<int>(combined_heightmap_[static_cast<size_t>(gy)].size()))
        return 0.0f;

    int h = combined_heightmap_[static_cast<size_t>(gy)][static_cast<size_t>(gx)];
    return static_cast<float>(h) * 0.1f;  // Height scale
}

// LevelManager implementation

void LevelManager::init(IRenderer* renderer)
{
    renderer_ = renderer;
    load_default_mappings();
}

void LevelManager::shutdown()
{
    unload_level();
    type_mappings_.clear();
    renderer_ = nullptr;
}

bool LevelManager::load_level(const std::filesystem::path& yaml_path)
{
    unload_level();

    if (!level_.load(yaml_path))
        return false;

    load_level_models();
    create_terrain_mesh();
    return true;
}

void LevelManager::unload_level()
{
    unload_level_models();
    level_.unload();

    if (terrain_mesh_ != invalid_mesh && renderer_)
    {
        renderer_->destroy_mesh(terrain_mesh_);
        terrain_mesh_ = invalid_mesh;
    }
}

std::vector<std::string> LevelManager::list_levels(const std::filesystem::path& maps_dir)
{
    std::vector<std::string> levels;

    try
    {
        for (const auto& entry : std::filesystem::directory_iterator(maps_dir))
        {
            if (entry.is_regular_file())
            {
                auto ext = entry.path().extension().string();
                if (ext == ".yaml" || ext == ".yml")
                {
                    auto name = entry.path().stem().string();
                    // Skip non-level files
                    if (name != "levels")
                        levels.push_back(name);
                }
            }
        }
        std::sort(levels.begin(), levels.end());
    }
    catch (const std::exception& e)
    {
        spdlog::error("Error listing levels: {}", e.what());
    }

    return levels;
}

void LevelManager::register_object_mapping(const object_type_mapping& mapping)
{
    type_mappings_[mapping.type_name] = mapping;
}

void LevelManager::load_default_mappings()
{
    // Helicopters
    register_object_mapping({ "oh1", "assets/models/helics/cobra/cobra.obj", "", 0.04f });
    register_object_mapping({ "k27", "assets/models/helics/kamov/kamov.obj", "", 0.04f });
    register_object_mapping({ "heli_uh_green", "assets/models/helics/mi_24/mi_24.obj", "", 0.04f });

    // Tanks
    register_object_mapping({ "t72_desert", "assets/models/tanks/tiger/tiger_base.obj", "", 0.04f });
    register_object_mapping({ "bt_2_desert", "assets/models/tanks/tiger/tiger_base.obj", "", 0.03f });
    register_object_mapping({ "bt_2_green", "assets/models/tanks/tiger/tiger_base.obj", "", 0.03f });

    // Vehicles - we'd need to add these models
    // For now, just register them so we know they exist
    
    spdlog::info("Loaded {} object type mappings", type_mappings_.size());
}

void LevelManager::load_level_models()
{
    if (!renderer_)
        return;

    // Find unique object types in level that we have mappings for
    std::unordered_map<std::string, bool> needed_types;
    for (const auto& obj : level_.objects())
    {
        if (type_mappings_.contains(obj.type_name))
            needed_types[obj.type_name] = true;
    }

    // Load models
    for (const auto& [type_name, _] : needed_types)
    {
        auto it = type_mappings_.find(type_name);
        if (it == type_mappings_.end())
            continue;

        const auto& mapping = it->second;
        auto model = renderer_->load_model(mapping.model_path);
        if (model != invalid_model)
        {
            loaded_models_[type_name] = { model, mapping.scale };
            spdlog::info("  Loaded model for: {}", type_name);
        }
    }
}

void LevelManager::unload_level_models()
{
    if (!renderer_)
        return;

    for (auto& [name, lm] : loaded_models_)
    {
        if (lm.model != invalid_model)
            renderer_->unload_model(lm.model);
    }
    loaded_models_.clear();
}

void LevelManager::create_terrain_mesh()
{
    if (!renderer_ || !level_.is_loaded())
        return;

    const auto& hmap = level_.heightmap();
    if (hmap.empty())
        return;

    // Create wireframe grid for terrain
    std::vector<vertex> verts;
    std::vector<uint16_t> indices;

    float scale = static_cast<float>(level_.header().terrain_scale) / 
                  static_cast<float>(level_.header().grid_size);
    float height_scale = 0.1f;

    int width  = level_.header().grid_size;
    int height = static_cast<int>(hmap.size());

    // Sample every N rows for performance (terrain can be huge)
    int step = std::max(1, height / 64);

    glm::vec3 color(0.2f, 0.4f, 0.2f);

    for (int y = 0; y < height; y += step)
    {
        for (int x = 0; x < width; ++x)
        {
            float h = static_cast<float>(hmap[static_cast<size_t>(y)][static_cast<size_t>(x)]) * height_scale;
            
            // Road is special (255 = flat)
            if (hmap[static_cast<size_t>(y)][static_cast<size_t>(x)] >= 250)
                h = 8.0f * height_scale;

            auto base = static_cast<uint16_t>(verts.size());
            verts.push_back({ glm::vec3(static_cast<float>(x) * scale, static_cast<float>(y) * scale, h), color });

            // Connect to previous point in row
            if (x > 0)
            {
                indices.push_back(static_cast<uint16_t>(base - 1));
                indices.push_back(base);
            }
        }
    }

    // Vertical lines (every 8 grid cells)
    for (int x = 0; x < width; x += 8)
    {
        uint16_t prev = static_cast<uint16_t>(-1);
        for (int y = 0; y < height; y += step)
        {
            float h = static_cast<float>(hmap[static_cast<size_t>(y)][static_cast<size_t>(x)]) * height_scale;
            if (hmap[static_cast<size_t>(y)][static_cast<size_t>(x)] >= 250)
                h = 8.0f * height_scale;

            auto base = static_cast<uint16_t>(verts.size());
            verts.push_back({ glm::vec3(static_cast<float>(x) * scale, static_cast<float>(y) * scale, h), color });

            if (prev != static_cast<uint16_t>(-1))
            {
                indices.push_back(prev);
                indices.push_back(base);
            }
            prev = base;
        }
    }

    if (!verts.empty() && !indices.empty())
    {
        terrain_mesh_ = renderer_->create_mesh(verts, indices, primitive_type::lines);
        spdlog::info("Created terrain mesh: {} verts, {} lines", verts.size(), indices.size() / 2);
    }
}

void LevelManager::render()
{
    if (!renderer_ || !level_.is_loaded())
        return;

    // Draw terrain
    if (terrain_mesh_ != invalid_mesh)
        renderer_->draw(terrain_mesh_);

    // Draw objects that we have models for
    for (const auto& obj : level_.objects())
    {
        auto it = loaded_models_.find(obj.type_name);
        if (it == loaded_models_.end())
            continue;

        const auto& lm = it->second;

        // Convert grid position to world position
        // Note: level Y is forward (scroll direction), we render as Z
        glm::vec3 pos(
            obj.position.x,
            0.0f,  // Height will be from terrain
            obj.position.y
        );

        // Get terrain height
        float h = level_.get_height_at(obj.position.x, obj.position.y);
        pos.y = h;

        renderer_->draw_model(lm.model, {
            .position = pos,
            .rotation = { 0.0f, obj.rotation, 0.0f },
            .scale    = glm::vec3(lm.scale)
        });
    }
}

} // namespace as3

