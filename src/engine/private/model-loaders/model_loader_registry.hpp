#pragma once

/// @file model_loader_registry.hpp
/// @brief Registry for model loaders supporting multiple formats

#include "gltf_loader.hpp"
#include "model_loader.hpp"
#include "obj_loader.hpp"

#include <algorithm>
#include <memory>
#include <ranges>
#include <vector>

namespace euengine
{

/// Registry managing multiple model loaders
class ModelLoaderRegistry final
{
public:
    ModelLoaderRegistry()
    {
        // Register built-in loaders
        register_loader(std::make_unique<ObjLoader>());
        register_loader(std::make_unique<GltfLoader>());
    }

    ~ModelLoaderRegistry() = default;

    // Non-copyable, movable
    ModelLoaderRegistry(const ModelLoaderRegistry&)            = delete;
    ModelLoaderRegistry& operator=(const ModelLoaderRegistry&) = delete;
    ModelLoaderRegistry(ModelLoaderRegistry&&)                 = default;
    ModelLoaderRegistry& operator=(ModelLoaderRegistry&&)      = default;

    /// Register a new loader
    void register_loader(std::unique_ptr<IModelLoader> loader)
    {
        if (loader)
            loaders_.push_back(std::move(loader));
    }

    /// Find a loader for the given file path
    [[nodiscard]] const IModelLoader* find_loader(
        const std::filesystem::path& path) const
    {
        auto ext = path.extension().string();
        std::ranges::transform(ext, ext.begin(), ::tolower);

        for (const auto& loader : loaders_)
        {
            if (loader->supports(ext))
                return loader.get();
        }
        return nullptr;
    }

    /// Load a model using the appropriate loader
    [[nodiscard]] load_result load(const std::filesystem::path& path) const
    {
        const auto* loader = find_loader(path);
        if (!loader)
        {
            return std::unexpected("no loader found for extension: " +
                                   path.extension().string());
        }
        return loader->load(path);
    }

    /// Get all supported extensions
    [[nodiscard]] std::vector<std::string_view> supported_extensions() const
    {
        std::vector<std::string_view> result;
        for (const auto& loader : loaders_)
        {
            auto exts = loader->extensions();
            result.insert(result.end(), exts.begin(), exts.end());
        }
        return result;
    }

private:
    std::vector<std::unique_ptr<IModelLoader>> loaders_;
};

/// Get the global model loader registry (singleton)
[[nodiscard]] inline ModelLoaderRegistry& get_model_loader_registry()
{
    static ModelLoaderRegistry registry;
    return registry;
}

} // namespace euengine
