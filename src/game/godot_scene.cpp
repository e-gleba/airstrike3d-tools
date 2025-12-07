#include "godot_scene.hpp"
#include "scene.hpp"
#include "ui.hpp"

#include <glm/gtc/constants.hpp>
#include <glm/vec3.hpp>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <format>
#include <fstream>
#include <map>
#include <regex>
#include <sstream>

namespace godot_scene
{

namespace
{

struct tscn_resource
{
    std::string                        id;
    std::string                        type;
    std::string                        path;
    std::map<std::string, std::string> properties;
};

struct tscn_node
{
    std::string                        name;
    std::string                        type;
    std::string                        parent;
    std::map<std::string, std::string> properties;
};

// Parse a Vector3 from Godot format: Vector3(x, y, z) or just "x, y, z"
glm::vec3 parse_vector3(const std::string& str)
{
    glm::vec3  result { 0.0f };
    std::regex vec_regex(
        R"(Vector3\s*\(\s*([-\d.]+)\s*,\s*([-\d.]+)\s*,\s*([-\d.]+)\s*\))");
    std::regex  simple_regex(R"(([-\d.]+)\s*,\s*([-\d.]+)\s*,\s*([-\d.]+))");
    std::smatch match;

    if (std::regex_search(str, match, vec_regex) ||
        std::regex_search(str, match, simple_regex))
    {
        result.x = std::stof(match[1].str());
        result.y = std::stof(match[2].str());
        result.z = std::stof(match[3].str());
    }
    return result;
}

// Parse Transform3D from Godot format
// Format: Transform3D(basis, origin) or Transform3D(12 floats)
bool parse_transform3d(const std::string& str,
                       glm::vec3&         pos,
                       glm::vec3&         rot,
                       glm::vec3&         scale)
{
    // Try to extract 12 floats from Transform3D(...)
    std::regex transform_regex(
        R"(Transform3D\s*\(\s*([-\d.]+)\s*,\s*([-\d.]+)\s*,\s*([-\d.]+)\s*,\s*([-\d.]+)\s*,\s*([-\d.]+)\s*,\s*([-\d.]+)\s*,\s*([-\d.]+)\s*,\s*([-\d.]+)\s*,\s*([-\d.]+)\s*,\s*([-\d.]+)\s*,\s*([-\d.]+)\s*,\s*([-\d.]+)\s*\))");
    std::smatch match;

    if (std::regex_search(str, match, transform_regex))
    {
        // Extract basis matrix (first 9 values) and origin (last 3)
        float m00 = std::stof(match[1].str());
        float m01 = std::stof(match[2].str());
        float m02 = std::stof(match[3].str());
        float m10 = std::stof(match[4].str());
        float m11 = std::stof(match[5].str());
        float m12 = std::stof(match[6].str());
        float m20 = std::stof(match[7].str());
        float m21 = std::stof(match[8].str());
        float m22 = std::stof(match[9].str());
        pos.x     = std::stof(match[10].str());
        pos.y     = std::stof(match[11].str());
        pos.z     = std::stof(match[12].str());

        // Extract scale from basis matrix (length of each column)
        scale.x = std::sqrt(m00 * m00 + m10 * m10 + m20 * m20);
        scale.y = std::sqrt(m01 * m01 + m11 * m11 + m21 * m21);
        scale.z = std::sqrt(m02 * m02 + m12 * m12 + m22 * m22);

        // Normalize basis for rotation extraction
        if (scale.x > 0.0001f)
        {
            m00 /= scale.x;
            m10 /= scale.x;
            m20 /= scale.x;
        }
        if (scale.y > 0.0001f)
        {
            m01 /= scale.y;
            m11 /= scale.y;
            m21 /= scale.y;
        }
        if (scale.z > 0.0001f)
        {
            m02 /= scale.z;
            m12 /= scale.z;
            m22 /= scale.z;
        }

        // Extract rotation (simplified - just use Y rotation for now)
        rot.y = std::atan2(m02, m22) * 180.0f / glm::pi<float>();
        rot.x = std::asin(-m12) * 180.0f / glm::pi<float>();
        rot.z = std::atan2(m10, m11) * 180.0f / glm::pi<float>();

        return true;
    }

    // Try separate position/rotation/scale properties
    return false;
}

// Extract resource ID from ExtResource("1") or SubResource("1")
std::string extract_resource_id(const std::string& str)
{
    std::regex res_regex(
        R"delim((?:Ext|Sub)Resource\s*\(\s*"([^"]+)"\s*\))delim");
    std::smatch match;
    if (std::regex_search(str, match, res_regex))
        return match[1].str();
    return "";
}

// Trim whitespace
std::string trim(const std::string& str)
{
    size_t first = str.find_first_not_of(" \t");
    if (first == std::string::npos)
        return "";
    size_t last = str.find_last_not_of(" \t");
    return str.substr(first, last - first + 1);
}

} // namespace

std::filesystem::path resolve_resource_path(
    const std::string& path, const std::filesystem::path& tscn_dir)
{
    if (path.empty())
        return {};

    // Handle res:// paths - resolve relative to TSCN file directory
    if (path.starts_with("res://"))
    {
        std::string rel_path = path.substr(6); // Remove "res://"
        // Remove leading slash if present
        if (!rel_path.empty() && rel_path[0] == '/')
            rel_path = rel_path.substr(1);
        return (tscn_dir / rel_path).lexically_normal();
    }

    // Handle absolute paths
    std::filesystem::path fs_path(path);
    if (fs_path.is_absolute())
        return fs_path.lexically_normal();

    // Handle relative paths - resolve relative to TSCN file directory
    return (tscn_dir / path).lexically_normal();
}

bool load_tscn(const std::string& tscn_path)
{
    spdlog::info("load_tscn called with path: {}", tscn_path);

    std::filesystem::path tscn_fs_path(tscn_path);

    // Resolve TSCN path to absolute
    if (!tscn_fs_path.is_absolute())
    {
        try
        {
            tscn_fs_path = std::filesystem::absolute(tscn_fs_path);
            spdlog::debug("Resolved relative path to absolute: {}",
                          tscn_fs_path.string());
        }
        catch (const std::exception& e)
        {
            spdlog::error("Failed to resolve absolute path for {}: {}",
                          tscn_path,
                          e.what());
            ui::log(4, "Failed to resolve path: " + std::string(e.what()));
            return false;
        }
    }

    if (!std::filesystem::exists(tscn_fs_path))
    {
        spdlog::error("TSCN file does not exist: {}", tscn_fs_path.string());
        ui::log(4, "TSCN file not found: " + tscn_path);
        return false;
    }

    if (!std::filesystem::is_regular_file(tscn_fs_path))
    {
        spdlog::error("TSCN path is not a regular file: {}",
                      tscn_fs_path.string());
        ui::log(4, "TSCN path is not a file: " + tscn_path);
        return false;
    }

    spdlog::info("Opening TSCN file: {}", tscn_fs_path.string());
    std::ifstream file(tscn_fs_path);
    if (!file.is_open())
    {
        spdlog::error("Failed to open TSCN file for reading: {}",
                      tscn_fs_path.string());
        ui::log(4, "Failed to open TSCN file: " + tscn_path);
        return false;
    }

    spdlog::info("TSCN file opened successfully, starting parse...");

    std::map<std::string, tscn_resource> ext_resources;
    std::map<std::string, tscn_resource> sub_resources;
    std::vector<tscn_node>               nodes;

    std::string   line;
    std::string   current_section;
    tscn_resource current_resource;
    tscn_node     current_node;

    auto tscn_dir = tscn_fs_path.parent_path();

    while (std::getline(file, line))
    {
        line = trim(line);
        if (line.empty() || line[0] == ';')
            continue; // Skip empty lines and comments

        // Check for section headers
        if (line[0] == '[' && line.back() == ']')
        {
            std::string header_content = line.substr(1, line.length() - 2);
            size_t      space_pos      = header_content.find(' ');

            if (space_pos == std::string::npos)
            {
                // Simple header like [gd_scene] or [node]
                current_section = header_content;
            }
            else
            {
                // Header with attributes like [node name="Node3D"
                // type="Node3D"]
                current_section = header_content.substr(0, space_pos);

                // Parse attributes from header line
                if (current_section == "node")
                {
                    if (!current_node.name.empty())
                        nodes.push_back(current_node);
                    current_node = tscn_node();

                    // Parse attributes: name="..." type="..." parent="..."
                    std::string attrs = header_content.substr(space_pos + 1);
                    std::regex  name_regex(R"delim(name\s*=\s*"([^"]+)")delim");
                    std::regex  type_regex(R"delim(type\s*=\s*"([^"]+)")delim");
                    std::regex  parent_regex(
                        R"delim(parent\s*=\s*"([^"]+)")delim");
                    std::smatch match;

                    if (std::regex_search(attrs, match, name_regex))
                        current_node.name = match[1].str();
                    if (std::regex_search(attrs, match, type_regex))
                        current_node.type = match[1].str();
                    if (std::regex_search(attrs, match, parent_regex))
                        current_node.parent = match[1].str();

                    spdlog::debug(
                        "Parsed node header: name='{}', type='{}', parent='{}'",
                        current_node.name,
                        current_node.type,
                        current_node.parent);
                }
                else if (current_section == "ext_resource")
                {
                    if (!current_resource.id.empty())
                        ext_resources[current_resource.id] = current_resource;
                    current_resource = tscn_resource();

                    // Parse attributes: type="..." path="..." id="..."
                    // uid="..." Note: id= must be matched carefully to not
                    // match uid=
                    std::string attrs = header_content.substr(space_pos + 1);
                    std::regex  type_regex(R"delim(type\s*=\s*"([^"]+)")delim");
                    std::regex  path_regex(R"delim(path\s*=\s*"([^"]+)")delim");
                    // Match id= but NOT uid= (use word boundary or space
                    // before)
                    std::regex id_regex(
                        R"delim((?:^|\s)id\s*=\s*"([^"]+)")delim");
                    std::smatch match;

                    if (std::regex_search(attrs, match, type_regex))
                        current_resource.type = match[1].str();
                    if (std::regex_search(attrs, match, path_regex))
                    {
                        std::string path_val = match[1].str();
                        auto        resolved =
                            resolve_resource_path(path_val, tscn_dir);
                        current_resource.path = resolved.string();
                        spdlog::debug(
                            "ext_resource path resolved: '{}' -> '{}'",
                            path_val,
                            current_resource.path);
                    }
                    if (std::regex_search(attrs, match, id_regex))
                    {
                        current_resource.id = match[1].str();
                        spdlog::debug("ext_resource id: '{}'",
                                      current_resource.id);
                    }

                    if (!current_resource.id.empty())
                    {
                        ext_resources[current_resource.id] = current_resource;
                        spdlog::info("Stored ext_resource: id='{}', type='{}', "
                                     "path='{}'",
                                     current_resource.id,
                                     current_resource.type,
                                     current_resource.path);
                    }
                }
                else if (current_section == "sub_resource")
                {
                    if (!current_resource.id.empty())
                        sub_resources[current_resource.id] = current_resource;
                    current_resource = tscn_resource();

                    // Parse attributes: type="..." id="..."
                    std::string attrs = header_content.substr(space_pos + 1);
                    std::regex  type_regex(R"delim(type\s*=\s*"([^"]+)")delim");
                    // Match id= but NOT uid=
                    std::regex id_regex(
                        R"delim((?:^|\s)id\s*=\s*"([^"]+)")delim");
                    std::smatch match;

                    if (std::regex_search(attrs, match, type_regex))
                        current_resource.type = match[1].str();
                    if (std::regex_search(attrs, match, id_regex))
                    {
                        current_resource.id = match[1].str();
                        spdlog::debug("sub_resource id: '{}'",
                                      current_resource.id);
                    }

                    if (!current_resource.id.empty())
                    {
                        sub_resources[current_resource.id] = current_resource;
                        spdlog::info("Stored sub_resource: id='{}', type='{}'",
                                     current_resource.id,
                                     current_resource.type);
                    }
                }
            }

            if (current_section == "ext_resource" &&
                space_pos == std::string::npos)
            {
                if (!current_resource.id.empty())
                    ext_resources[current_resource.id] = current_resource;
                current_resource = tscn_resource();
            }
            else if (current_section == "sub_resource" &&
                     space_pos == std::string::npos)
            {
                if (!current_resource.id.empty())
                    sub_resources[current_resource.id] = current_resource;
                current_resource = tscn_resource();
            }
            else if (current_section == "node" &&
                     space_pos == std::string::npos)
            {
                if (!current_node.name.empty())
                    nodes.push_back(current_node);
                current_node = tscn_node();
            }
            continue;
        }

        // Parse key=value pairs
        size_t eq_pos = line.find('=');
        if (eq_pos == std::string::npos)
            continue;

        std::string key   = trim(line.substr(0, eq_pos));
        std::string value = trim(line.substr(eq_pos + 1));

        // Remove quotes if present
        if (value.size() >= 2 && value[0] == '"' && value.back() == '"')
            value = value.substr(1, value.length() - 2);

        if (current_section == "ext_resource")
        {
            if (key == "id")
                current_resource.id = value;
            else if (key == "type")
                current_resource.type = value;
            else if (key == "path")
            {
                // Resolve path using res:// resolution
                auto resolved         = resolve_resource_path(value, tscn_dir);
                current_resource.path = resolved.string();
            }

            if (!current_resource.id.empty())
                ext_resources[current_resource.id] = current_resource;
        }
        else if (current_section == "sub_resource")
        {
            if (key == "id")
                current_resource.id = value;
            else if (key == "type")
                current_resource.type = value;
            else
                current_resource.properties[key] = value;

            if (!current_resource.id.empty())
                sub_resources[current_resource.id] = current_resource;
        }
        else if (current_section == "node")
        {
            if (key == "name")
                current_node.name = value;
            else if (key == "type")
                current_node.type = value;
            else if (key == "parent")
                current_node.parent = value;
            else
                current_node.properties[key] = value;
        }
    }

    // Add last resource and node
    if (current_section == "ext_resource" && !current_resource.id.empty())
        ext_resources[current_resource.id] = current_resource;
    if (current_section == "sub_resource" && !current_resource.id.empty())
        sub_resources[current_resource.id] = current_resource;
    if (!current_node.name.empty())
        nodes.push_back(current_node);

    spdlog::info("Parsed TSCN: {} ext_resources, {} sub_resources, {} nodes",
                 ext_resources.size(),
                 sub_resources.size(),
                 nodes.size());

    // Debug: log all nodes found
    for (const auto& node : nodes)
    {
        spdlog::info("Found node: name='{}', type='{}', parent='{}'",
                     node.name,
                     node.type,
                     node.parent);
    }

    // Debug: log all ext_resources
    for (const auto& [id, res] : ext_resources)
    {
        spdlog::info("ExtResource: id='{}', type='{}', path='{}'",
                     id,
                     res.type,
                     res.path);
    }

    // Process nodes - find MeshInstance3D nodes
    int loaded_count        = 0;
    int mesh_instance_count = 0;
    spdlog::info("Processing {} nodes to find MeshInstance3D...", nodes.size());

    for (const auto& node : nodes)
    {
        spdlog::debug(
            "Checking node: type='{}', name='{}'", node.type, node.name);
        if (node.type != "MeshInstance3D")
        {
            spdlog::debug(
                "Skipping node '{}' (type: '{}')", node.name, node.type);
            continue;
        }
        mesh_instance_count++;

        spdlog::info("Found MeshInstance3D node: {}", node.name);

        // Log all properties for debugging
        spdlog::info(
            "Node '{}' has {} properties:", node.name, node.properties.size());
        for (const auto& [k, v] : node.properties)
        {
            spdlog::info("  Property: '{}' = '{}'", k, v);
        }

        // Find mesh resource
        auto mesh_it = node.properties.find("mesh");
        if (mesh_it == node.properties.end())
        {
            spdlog::error("MeshInstance3D '{}' has no mesh property! Available "
                          "properties:",
                          node.name);
            for (const auto& [k, v] : node.properties)
            {
                spdlog::error("  '{}' = '{}'", k, v);
            }
            continue;
        }

        spdlog::info("Mesh property value: '{}'", mesh_it->second);

        std::string mesh_res_id = extract_resource_id(mesh_it->second);
        if (mesh_res_id.empty())
        {
            spdlog::error(
                "MeshInstance3D '{}' has invalid mesh resource ID from: '{}'",
                node.name,
                mesh_it->second);
            continue;
        }

        spdlog::info("MeshInstance3D '{}' references mesh resource ID: '{}'",
                     node.name,
                     mesh_res_id);

        // Look up resource
        spdlog::info("Looking for resource ID '{}' in {} ext_resources, {} "
                     "sub_resources",
                     mesh_res_id,
                     ext_resources.size(),
                     sub_resources.size());

        std::string mesh_path;
        auto        ext_it = ext_resources.find(mesh_res_id);
        if (ext_it != ext_resources.end())
        {
            mesh_path = ext_it->second.path;
            spdlog::info("Found mesh in ext_resource '{}': path='{}'",
                         mesh_res_id,
                         mesh_path);
        }
        else
        {
            spdlog::info(
                "Not found in ext_resources, checking sub_resources...");
            auto sub_it = sub_resources.find(mesh_res_id);
            if (sub_it != sub_resources.end())
            {
                spdlog::info("Found mesh in sub_resource '{}' (type: {})",
                             mesh_res_id,
                             sub_it->second.type);
                // For sub_resources, try to find the mesh path in properties
                auto path_it = sub_it->second.properties.find("resource_path");
                if (path_it != sub_it->second.properties.end())
                {
                    auto resolved =
                        resolve_resource_path(path_it->second, tscn_dir);
                    mesh_path = resolved.string();
                    spdlog::info("Resolved sub_resource path: {}", mesh_path);
                }
                else
                {
                    spdlog::error(
                        "sub_resource '{}' has no resource_path property",
                        mesh_res_id);
                }
            }
            else
            {
                spdlog::error("Mesh resource ID '{}' not found in "
                              "ext_resources or sub_resources!",
                              mesh_res_id);
                spdlog::info("Available ext_resources:");
                for (const auto& [id, res] : ext_resources)
                {
                    spdlog::info("  '{}' -> '{}'", id, res.path);
                }
            }
        }

        if (mesh_path.empty())
        {
            spdlog::error("MeshInstance3D '{}' has empty mesh path, skipping",
                          node.name);
            continue;
        }

        spdlog::info("Checking if mesh file exists: {}", mesh_path);
        if (!std::filesystem::exists(mesh_path))
        {
            spdlog::error("Mesh file does not exist: {}", mesh_path);
            // Try to load anyway - maybe the engine can find it
            spdlog::info("Attempting to load anyway...");
        }

        spdlog::info("Loading mesh for '{}': {}", node.name, mesh_path);

        // Parse transform
        glm::vec3 pos { 0.0f };
        glm::vec3 rot { 0.0f };
        glm::vec3 scale { 1.0f };

        // Try transform property first
        auto transform_it = node.properties.find("transform");
        if (transform_it != node.properties.end())
        {
            parse_transform3d(transform_it->second, pos, rot, scale);
        }
        else
        {
            // Try separate position/rotation/scale
            auto pos_it = node.properties.find("position");
            if (pos_it != node.properties.end())
                pos = parse_vector3(pos_it->second);

            auto rot_it = node.properties.find("rotation");
            if (rot_it != node.properties.end())
            {
                glm::vec3 rot_rad = parse_vector3(rot_it->second);
                rot               = rot_rad * 180.0f /
                      glm::pi<float>(); // Convert radians to degrees
            }

            auto scale_it = node.properties.find("scale");
            if (scale_it != node.properties.end())
                scale = parse_vector3(scale_it->second);
        }

        // Convert Godot coordinates to engine coordinates
        // Godot: Y-up, -Z forward (into screen), +X right
        // Engine: Y-up, +Z toward camera, -X right (left-handed vs
        // right-handed) Negate both X and Z to convert coordinate systems
        pos.x = -pos.x;
        pos.z = -pos.z;

        spdlog::info("Transformed position: ({}, {}, {})", pos.x, pos.y, pos.z);

        // Load model
        float avg_scale = (scale.x + scale.y + scale.z) / 3.0f;
        if (avg_scale < 0.001f)
            avg_scale = 1.0f;

        if (auto* m = scene::add_model(mesh_path, pos, avg_scale))
        {
            m->name               = node.name;
            m->transform.rotation = rot;
            m->transform.scale    = scale;
            loaded_count++;
            spdlog::info(
                "Loaded model '{}' from mesh: {}", node.name, mesh_path);
        }
        else
        {
            spdlog::error("Failed to load model '{}' from mesh: {}",
                          node.name,
                          mesh_path);
        }
    }

    spdlog::info("TSCN parsing complete: {} MeshInstance3D nodes found, {} "
                 "successfully loaded",
                 mesh_instance_count,
                 loaded_count);

    if (loaded_count > 0)
    {
        spdlog::info("Successfully loaded {} objects from TSCN: {}",
                     loaded_count,
                     tscn_fs_path.filename().string());
        ui::log(2,
                std::format("Loaded {} objects from TSCN: {}",
                            loaded_count,
                            tscn_fs_path.filename().string()));
    }
    else
    {
        spdlog::warn("No objects loaded from TSCN file: {} (found {} total "
                     "nodes, {} MeshInstance3D nodes)",
                     tscn_fs_path.filename().string(),
                     nodes.size(),
                     mesh_instance_count);
        ui::log(3,
                std::format("No objects loaded from TSCN: {} (found {} "
                            "MeshInstance3D nodes)",
                            tscn_fs_path.filename().string(),
                            mesh_instance_count));
    }
    return loaded_count > 0;
}

} // namespace godot_scene
