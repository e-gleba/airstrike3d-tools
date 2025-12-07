#include "gltf_scene.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <spdlog/spdlog.h>
#include <tiny_gltf.h>

#include <algorithm>
#include <ranges>
#include <span>
#include <stack>

namespace gltf_scene
{

glm::mat4 scene_transform::matrix() const
{
    glm::mat4 m { 1.0f };
    m = glm::translate(m, position);
    m = m * glm::mat4_cast(rotation);
    m = glm::scale(m, scale);
    return m;
}

namespace
{

/// Extract typed data from glTF accessor
template <typename T>
[[nodiscard]] std::span<const T> get_accessor_data(
    const tinygltf::Model&    model,
    const tinygltf::Accessor& accessor)
{
    const auto& view =
        model.bufferViews[static_cast<size_t>(accessor.bufferView)];
    const auto& buffer = model.buffers[static_cast<size_t>(view.buffer)];
    const auto* data   = reinterpret_cast<const T*>(
        buffer.data.data() + view.byteOffset + accessor.byteOffset);
    return { data, accessor.count };
}

/// Read indices with various component types
[[nodiscard]] std::vector<uint32_t> read_indices(
    const tinygltf::Model&    model,
    const tinygltf::Accessor& accessor)
{
    std::vector<uint32_t> indices;
    indices.reserve(accessor.count);

    const auto& view =
        model.bufferViews[static_cast<size_t>(accessor.bufferView)];
    const auto& buffer = model.buffers[static_cast<size_t>(view.buffer)];
    const auto* data   = buffer.data.data() + view.byteOffset + accessor.byteOffset;

    for (size_t i = 0; i < accessor.count; ++i)
    {
        uint32_t idx = 0;
        switch (accessor.componentType)
        {
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                idx = static_cast<const uint8_t*>(
                    static_cast<const void*>(data))[i];
                break;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                idx = static_cast<const uint16_t*>(
                    static_cast<const void*>(data))[i];
                break;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                idx = static_cast<const uint32_t*>(
                    static_cast<const void*>(data))[i];
                break;
            default:
                break;
        }
        indices.push_back(idx);
    }
    return indices;
}

/// Extract node transform
[[nodiscard]] scene_transform extract_transform(const tinygltf::Node& node,
                                                 const load_options&   opts)
{
    scene_transform t;

    if (!node.matrix.empty())
    {
        // Decompose matrix
        glm::mat4 m = glm::make_mat4(node.matrix.data());
        
        // Extract translation
        t.position = glm::vec3(m[3]);
        
        // Extract scale
        t.scale.x = glm::length(glm::vec3(m[0]));
        t.scale.y = glm::length(glm::vec3(m[1]));
        t.scale.z = glm::length(glm::vec3(m[2]));
        
        // Extract rotation
        glm::mat3 rot_mat {
            glm::vec3(m[0]) / t.scale.x,
            glm::vec3(m[1]) / t.scale.y,
            glm::vec3(m[2]) / t.scale.z
        };
        t.rotation = glm::quat_cast(rot_mat);
    }
    else
    {
        if (!node.translation.empty())
        {
            t.position = glm::vec3(
                static_cast<float>(node.translation[0]),
                static_cast<float>(node.translation[1]),
                static_cast<float>(node.translation[2]));
        }
        if (!node.rotation.empty())
        {
            // glTF quaternion: [x, y, z, w]
            t.rotation = glm::quat(
                static_cast<float>(node.rotation[3]), // w
                static_cast<float>(node.rotation[0]), // x
                static_cast<float>(node.rotation[1]), // y
                static_cast<float>(node.rotation[2])  // z
            );
        }
        if (!node.scale.empty())
        {
            t.scale = glm::vec3(
                static_cast<float>(node.scale[0]),
                static_cast<float>(node.scale[1]),
                static_cast<float>(node.scale[2]));
        }
    }

    // Apply global scale
    t.position *= opts.global_scale;
    t.scale *= opts.global_scale;

    return t;
}

/// Load material from glTF
[[nodiscard]] scene_material load_material(
    const tinygltf::Model&    model,
    const tinygltf::Material& mat,
    const std::filesystem::path& base_path)
{
    scene_material m;
    m.name = mat.name;

    // PBR metallic-roughness
    const auto& pbr = mat.pbrMetallicRoughness;
    m.base_color = glm::vec4(
        static_cast<float>(pbr.baseColorFactor[0]),
        static_cast<float>(pbr.baseColorFactor[1]),
        static_cast<float>(pbr.baseColorFactor[2]),
        static_cast<float>(pbr.baseColorFactor[3]));
    m.metallic  = static_cast<float>(pbr.metallicFactor);
    m.roughness = static_cast<float>(pbr.roughnessFactor);

    // Base color texture
    if (pbr.baseColorTexture.index >= 0)
    {
        const auto& tex =
            model.textures[static_cast<size_t>(pbr.baseColorTexture.index)];
        if (tex.source >= 0)
        {
            const auto& img = model.images[static_cast<size_t>(tex.source)];
            if (!img.uri.empty())
            {
                m.base_color_texture = base_path / img.uri;
            }
        }
    }

    // Metallic-roughness texture
    if (pbr.metallicRoughnessTexture.index >= 0)
    {
        const auto& tex = model.textures[static_cast<size_t>(
            pbr.metallicRoughnessTexture.index)];
        if (tex.source >= 0)
        {
            const auto& img = model.images[static_cast<size_t>(tex.source)];
            if (!img.uri.empty())
            {
                m.metallic_roughness_texture = base_path / img.uri;
            }
        }
    }

    // Normal map
    if (mat.normalTexture.index >= 0)
    {
        const auto& tex =
            model.textures[static_cast<size_t>(mat.normalTexture.index)];
        if (tex.source >= 0)
        {
            const auto& img = model.images[static_cast<size_t>(tex.source)];
            if (!img.uri.empty())
            {
                m.normal_texture = base_path / img.uri;
            }
        }
    }

    // Emissive
    if (!mat.emissiveFactor.empty())
    {
        m.emissive_factor = glm::vec3(
            static_cast<float>(mat.emissiveFactor[0]),
            static_cast<float>(mat.emissiveFactor[1]),
            static_cast<float>(mat.emissiveFactor[2]));
    }
    if (mat.emissiveTexture.index >= 0)
    {
        const auto& tex =
            model.textures[static_cast<size_t>(mat.emissiveTexture.index)];
        if (tex.source >= 0)
        {
            const auto& img = model.images[static_cast<size_t>(tex.source)];
            if (!img.uri.empty())
            {
                m.emissive_texture = base_path / img.uri;
            }
        }
    }

    m.double_sided = mat.doubleSided;
    m.alpha_cutoff = static_cast<float>(mat.alphaCutoff);
    
    if (mat.alphaMode == "MASK")
        m.alpha = scene_material::alpha_mode::mask;
    else if (mat.alphaMode == "BLEND")
        m.alpha = scene_material::alpha_mode::blend;
    else
        m.alpha = scene_material::alpha_mode::opaque;

    return m;
}

/// Load mesh primitive
[[nodiscard]] scene_primitive load_primitive(
    const tinygltf::Model&     model,
    const tinygltf::Primitive& prim,
    const load_options&        opts)
{
    scene_primitive result;
    result.material_index = prim.material;

    // Positions (required)
    if (auto it = prim.attributes.find("POSITION"); it != prim.attributes.end())
    {
        const auto& accessor =
            model.accessors[static_cast<size_t>(it->second)];
        auto data = get_accessor_data<glm::vec3>(model, accessor);
        result.positions.reserve(data.size());
        
        for (const auto& p : data)
        {
            result.positions.push_back(p * opts.global_scale);
        }
    }

    // Normals
    if (auto it = prim.attributes.find("NORMAL"); it != prim.attributes.end())
    {
        const auto& accessor =
            model.accessors[static_cast<size_t>(it->second)];
        auto data = get_accessor_data<glm::vec3>(model, accessor);
        result.normals.assign(data.begin(), data.end());
    }

    // Texture coordinates
    if (auto it = prim.attributes.find("TEXCOORD_0");
        it != prim.attributes.end())
    {
        const auto& accessor =
            model.accessors[static_cast<size_t>(it->second)];
        auto data = get_accessor_data<glm::vec2>(model, accessor);
        result.texcoords.reserve(data.size());
        
        for (const auto& tc : data)
        {
            glm::vec2 uv = tc;
            if (opts.flip_texcoords_v)
                uv.y = 1.0f - uv.y;
            result.texcoords.push_back(uv);
        }
    }

    // Vertex colors
    if (auto it = prim.attributes.find("COLOR_0"); it != prim.attributes.end())
    {
        const auto& accessor =
            model.accessors[static_cast<size_t>(it->second)];
        
        if (accessor.type == TINYGLTF_TYPE_VEC4)
        {
            auto data = get_accessor_data<glm::vec4>(model, accessor);
            result.colors.assign(data.begin(), data.end());
        }
        else if (accessor.type == TINYGLTF_TYPE_VEC3)
        {
            auto data = get_accessor_data<glm::vec3>(model, accessor);
            result.colors.reserve(data.size());
            for (const auto& c : data)
                result.colors.emplace_back(c, 1.0f);
        }
    }

    // Tangents (for normal mapping)
    if (auto it = prim.attributes.find("TANGENT"); it != prim.attributes.end())
    {
        const auto& accessor =
            model.accessors[static_cast<size_t>(it->second)];
        auto data = get_accessor_data<glm::vec4>(model, accessor);
        result.tangents.assign(data.begin(), data.end());
    }

    // Indices
    if (prim.indices >= 0)
    {
        const auto& accessor =
            model.accessors[static_cast<size_t>(prim.indices)];
        result.indices = read_indices(model, accessor);
    }
    else
    {
        // Generate sequential indices
        result.indices.resize(result.positions.size());
        std::ranges::iota(result.indices, 0u);
    }

    return result;
}

/// Load light from KHR_lights_punctual extension
[[nodiscard]] scene_light load_light(const tinygltf::Light& light)
{
    scene_light l;
    l.name = light.name;
    
    if (!light.color.empty())
    {
        l.color = glm::vec3(
            static_cast<float>(light.color[0]),
            static_cast<float>(light.color[1]),
            static_cast<float>(light.color[2]));
    }
    
    l.intensity = static_cast<float>(light.intensity);
    l.range     = static_cast<float>(light.range);

    if (light.type == "directional")
        l.light_type = scene_light::type::directional;
    else if (light.type == "spot")
    {
        l.light_type       = scene_light::type::spot;
        l.inner_cone_angle = static_cast<float>(light.spot.innerConeAngle);
        l.outer_cone_angle = static_cast<float>(light.spot.outerConeAngle);
    }
    else
        l.light_type = scene_light::type::point;

    return l;
}

/// Load camera from glTF
[[nodiscard]] scene_camera load_camera(const tinygltf::Camera& cam)
{
    scene_camera c;
    c.name = cam.name;
    
    if (cam.type == "perspective")
    {
        c.is_perspective = true;
        c.fov = static_cast<float>(cam.perspective.yfov) * 180.0f / 3.14159f;
        c.aspect = static_cast<float>(cam.perspective.aspectRatio);
        c.near_plane = static_cast<float>(cam.perspective.znear);
        c.far_plane = static_cast<float>(cam.perspective.zfar);
    }
    else
    {
        c.is_perspective = false;
        c.ortho_size = static_cast<float>(cam.orthographic.ymag);
        c.near_plane = static_cast<float>(cam.orthographic.znear);
        c.far_plane  = static_cast<float>(cam.orthographic.zfar);
    }

    return c;
}

/// Compute world transforms for all nodes
void compute_world_transforms(loaded_scene& scene)
{
    // Process nodes in hierarchy order
    std::stack<std::pair<int, glm::mat4>> stack;
    
    // Start with root nodes
    for (int root : scene.root_nodes)
    {
        stack.push({ root, glm::mat4 { 1.0f } });
    }

    while (!stack.empty())
    {
        auto [idx, parent_world] = stack.top();
        stack.pop();

        auto& node        = scene.nodes[static_cast<size_t>(idx)];
        node.world_matrix = parent_world * node.transform.matrix();

        for (int child : node.children)
        {
            stack.push({ child, node.world_matrix });
        }
    }
}

} // namespace

load_result load(const std::filesystem::path& path, const load_options& options)
{
    load_result result;

    if (!std::filesystem::exists(path))
    {
        result.error = "File not found: " + path.string();
        return result;
    }

    tinygltf::Model    gltf;
    tinygltf::TinyGLTF loader;
    std::string        err, warn;

    const auto ext = path.extension().string();
    bool       ok  = false;

    if (ext == ".glb" || ext == ".GLB")
    {
        ok = loader.LoadBinaryFromFile(&gltf, &err, &warn, path.string());
    }
    else
    {
        ok = loader.LoadASCIIFromFile(&gltf, &err, &warn, path.string());
    }

    if (!warn.empty())
    {
        result.warnings.push_back(warn);
        spdlog::warn("glTF warning: {}", warn);
    }

    if (!ok)
    {
        result.error = "glTF parse error: " + err;
        spdlog::error("glTF error: {}", err);
        return result;
    }

    loaded_scene scene;
    scene.name        = path.stem().string();
    scene.source_path = path;
    const auto base_path = path.parent_path();

    // Load materials
    if (options.load_materials)
    {
        scene.materials.reserve(gltf.materials.size());
        for (const auto& mat : gltf.materials)
        {
            scene.materials.push_back(load_material(gltf, mat, base_path));
        }
        spdlog::info("Loaded {} materials", scene.materials.size());
    }

    // Load meshes
    scene.meshes.reserve(gltf.meshes.size());
    for (const auto& mesh : gltf.meshes)
    {
        scene_mesh sm;
        sm.name = mesh.name;
        sm.primitives.reserve(mesh.primitives.size());

        for (const auto& prim : mesh.primitives)
        {
            if (prim.mode != TINYGLTF_MODE_TRIANGLES)
            {
                result.warnings.push_back(
                    "Skipping non-triangle primitive in mesh: " + mesh.name);
                continue;
            }
            sm.primitives.push_back(load_primitive(gltf, prim, options));
        }

        scene.meshes.push_back(std::move(sm));
    }
    spdlog::info("Loaded {} meshes", scene.meshes.size());

    // Load lights (KHR_lights_punctual)
    if (options.load_lights)
    {
        scene.lights.reserve(gltf.lights.size());
        for (const auto& light : gltf.lights)
        {
            scene.lights.push_back(load_light(light));
        }
        if (!scene.lights.empty())
            spdlog::info("Loaded {} lights", scene.lights.size());
    }

    // Load cameras
    if (options.load_cameras)
    {
        scene.cameras.reserve(gltf.cameras.size());
        for (const auto& cam : gltf.cameras)
        {
            scene.cameras.push_back(load_camera(cam));
        }
        if (!scene.cameras.empty())
            spdlog::info("Loaded {} cameras", scene.cameras.size());
    }

    // Load nodes
    scene.nodes.reserve(gltf.nodes.size());
    for (size_t i = 0; i < gltf.nodes.size(); ++i)
    {
        const auto& node = gltf.nodes[i];
        scene_node  sn;
        sn.name      = node.name;
        sn.transform = extract_transform(node, options);
        sn.mesh_index = node.mesh;

        // Check for light extension
        if (auto it = node.extensions.find("KHR_lights_punctual");
            it != node.extensions.end() && it->second.Has("light"))
        {
            sn.light_index = it->second.Get("light").GetNumberAsInt();
        }

        sn.camera_index = node.camera;

        // Children
        sn.children.reserve(node.children.size());
        for (int child : node.children)
        {
            sn.children.push_back(child);
        }

        scene.nodes.push_back(std::move(sn));
    }

    // Set parent references
    for (size_t i = 0; i < scene.nodes.size(); ++i)
    {
        for (int child : scene.nodes[i].children)
        {
            if (child >= 0 && static_cast<size_t>(child) < scene.nodes.size())
            {
                scene.nodes[static_cast<size_t>(child)].parent =
                    static_cast<int>(i);
            }
        }
    }

    // Find root nodes (nodes without parents)
    for (size_t i = 0; i < scene.nodes.size(); ++i)
    {
        if (scene.nodes[i].parent < 0)
        {
            scene.root_nodes.push_back(static_cast<int>(i));
        }
    }

    // Also check the default scene for root nodes
    if (gltf.defaultScene >= 0)
    {
        const auto& default_scene =
            gltf.scenes[static_cast<size_t>(gltf.defaultScene)];
        if (!default_scene.nodes.empty())
        {
            scene.root_nodes.clear();
            scene.root_nodes.reserve(default_scene.nodes.size());
            for (int node : default_scene.nodes)
            {
                scene.root_nodes.push_back(node);
            }
        }
    }

    spdlog::info("Loaded {} nodes ({} roots)",
                 scene.nodes.size(),
                 scene.root_nodes.size());

    // Compute world transforms
    if (options.compute_world_transforms)
    {
        compute_world_transforms(scene);
    }

    result.scene = std::move(scene);
    return result;
}

std::vector<std::filesystem::path> scan_directory(
    const std::filesystem::path& directory,
    bool                         recursive)
{
    std::vector<std::filesystem::path> files;

    if (!std::filesystem::exists(directory))
        return files;

    auto process = [&files](const std::filesystem::path& p)
    {
        if (!std::filesystem::is_regular_file(p))
            return;
        auto ext = p.extension().string();
        std::ranges::transform(ext, ext.begin(), ::tolower);
        if (ext == ".gltf" || ext == ".glb")
            files.push_back(p);
    };

    if (recursive)
    {
        for (const auto& entry :
             std::filesystem::recursive_directory_iterator(directory))
        {
            process(entry.path());
        }
    }
    else
    {
        for (const auto& entry : std::filesystem::directory_iterator(directory))
        {
            process(entry.path());
        }
    }

    std::ranges::sort(files);
    return files;
}

} // namespace gltf_scene

