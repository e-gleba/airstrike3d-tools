#pragma once

#include <core-api/renderer.hpp>

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>

#include <expected>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace euengine
{

enum class ShaderStage
{
    Vertex,
    Fragment
};

struct ShaderSource
{
    std::filesystem::path path;
    ShaderStage           stage;
    std::string           entry_point = "main";
};

struct ShaderProgramDesc
{
    std::string_view name;
    ShaderSource     vertex;
    ShaderSource     fragment;
};

class ShaderProgram final
{
public:
    ShaderProgram()                                = default;
    ~ShaderProgram()                               = default;
    ShaderProgram(const ShaderProgram&)            = delete;
    ShaderProgram& operator=(const ShaderProgram&) = delete;
    ShaderProgram(ShaderProgram&&) noexcept;
    ShaderProgram& operator=(ShaderProgram&&) noexcept;

    [[nodiscard]] SDL_GPUShader* vertex_shader() const noexcept
    {
        return vertex_shader_;
    }
    [[nodiscard]] SDL_GPUShader* fragment_shader() const noexcept
    {
        return fragment_shader_;
    }
    [[nodiscard]] bool valid() const noexcept
    {
        return vertex_shader_ && fragment_shader_;
    }

private:
    friend class ShaderManager;
    SDL_GPUShader* vertex_shader_   = nullptr;
    SDL_GPUShader* fragment_shader_ = nullptr;
    SDL_GPUDevice* device_          = nullptr;
    ShaderSource   vertex_source_;
    ShaderSource   fragment_source_;
    std::string    name_;
    SDL_Time       vertex_mod_time_   = 0;
    SDL_Time       fragment_mod_time_ = 0;
    void           release() noexcept;
};

class ShaderManager final : public i_shader_manager
{
public:
    using ReloadCallback = std::function<void(const std::string&)>;

    explicit ShaderManager(SDL_GPUDevice* device) noexcept;
    ~ShaderManager() override;
    ShaderManager(const ShaderManager&)            = delete;
    ShaderManager& operator=(const ShaderManager&) = delete;

    [[nodiscard]] std::expected<ShaderProgram*, std::string> load_program(
        const ShaderProgramDesc& desc);
    [[nodiscard]] ShaderProgram* get_program(std::string_view name) noexcept;

    void set_shader_directory(const std::filesystem::path& dir) noexcept;
    void check_for_updates();
    void set_reload_callback(ReloadCallback cb) noexcept;

    void enable_hot_reload(bool e) noexcept override
    {
        hot_reload_enabled_ = e;
    }
    [[nodiscard]] bool hot_reload_enabled() const noexcept override
    {
        return hot_reload_enabled_;
    }
    void release_all() noexcept;

private:
    [[nodiscard]] std::expected<SDL_GPUShader*, std::string> compile_shader(
        const ShaderSource& src);
    [[nodiscard]] std::expected<std::string, std::string> read_file(
        const std::filesystem::path& p) const;
    [[nodiscard]] SDL_Time get_mod_time(
        const std::filesystem::path& p) const noexcept;
    bool reload_program(ShaderProgram& prog);

    SDL_GPUDevice*                                 device_ = nullptr;
    std::filesystem::path                          shader_dir_;
    std::unordered_map<std::string, ShaderProgram> programs_;
    ReloadCallback                                 reload_callback_;
    bool                                           hot_reload_enabled_ = true;
};

} // namespace euengine
