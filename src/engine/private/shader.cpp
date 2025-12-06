#include "shader.hpp"

#include <SDL3_shadercross/SDL_shadercross.h>
#include <spdlog/spdlog.h>

#include <fstream>
#include <sstream>
#include <utility>

namespace euengine
{

ShaderProgram::ShaderProgram(ShaderProgram&& other) noexcept
    : vertex_shader_(std::exchange(other.vertex_shader_, nullptr))
    , fragment_shader_(std::exchange(other.fragment_shader_, nullptr))
    , device_(std::exchange(other.device_, nullptr))
    , vertex_source_(std::move(other.vertex_source_))
    , fragment_source_(std::move(other.fragment_source_))
    , name_(std::move(other.name_))
    , vertex_mod_time_(other.vertex_mod_time_)
    , fragment_mod_time_(other.fragment_mod_time_)
{
}

ShaderProgram& ShaderProgram::operator=(ShaderProgram&& other) noexcept
{
    if (this != &other)
    {
        release();
        vertex_shader_     = std::exchange(other.vertex_shader_, nullptr);
        fragment_shader_   = std::exchange(other.fragment_shader_, nullptr);
        device_            = std::exchange(other.device_, nullptr);
        vertex_source_     = std::move(other.vertex_source_);
        fragment_source_   = std::move(other.fragment_source_);
        name_              = std::move(other.name_);
        vertex_mod_time_   = other.vertex_mod_time_;
        fragment_mod_time_ = other.fragment_mod_time_;
    }
    return *this;
}

void ShaderProgram::release() noexcept
{
    if (device_ != nullptr)
    {
        if (vertex_shader_ != nullptr)
        {
            SDL_ReleaseGPUShader(device_, vertex_shader_);
            vertex_shader_ = nullptr;
        }
        if (fragment_shader_ != nullptr)
        {
            SDL_ReleaseGPUShader(device_, fragment_shader_);
            fragment_shader_ = nullptr;
        }
    }
}

ShaderManager::ShaderManager(SDL_GPUDevice* device) noexcept
    : device_(device)
{
    if (!SDL_ShaderCross_Init())
    {
        spdlog::error("Failed to init SDL_ShaderCross: {}", SDL_GetError());
    }
}

ShaderManager::~ShaderManager()
{
    release_all();
    SDL_ShaderCross_Quit();
}

void ShaderManager::set_shader_directory(
    const std::filesystem::path& dir) noexcept
{
    shader_dir_ = dir;
    spdlog::info("Shader directory set to: {}", shader_dir_.string());
}

void ShaderManager::set_reload_callback(ReloadCallback callback) noexcept
{
    reload_callback_ = std::move(callback);
}

std::expected<std::string, std::string> ShaderManager::read_file(
    const std::filesystem::path& path) const
{
    std::filesystem::path full_path = path;
    if (path.is_relative() && !shader_dir_.empty())
    {
        full_path = shader_dir_ / path;
    }

    std::ifstream file(full_path, std::ios::in);
    if (!file.is_open())
    {
        return std::unexpected(
            std::format("Failed to open shader file: {}", full_path.string()));
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

SDL_Time ShaderManager::get_mod_time(
    const std::filesystem::path& path) const noexcept
{
    std::filesystem::path full_path = path;
    if (path.is_relative() && !shader_dir_.empty())
    {
        full_path = shader_dir_ / path;
    }

    SDL_PathInfo info {};
    if (SDL_GetPathInfo(full_path.c_str(), &info))
    {
        return info.modify_time;
    }
    return 0;
}

std::expected<SDL_GPUShader*, std::string> ShaderManager::compile_shader(
    const ShaderSource& source)
{
    auto content_result = read_file(source.path);
    if (!content_result)
    {
        return std::unexpected(content_result.error());
    }

    const SDL_ShaderCross_ShaderStage stage =
        source.stage == ShaderStage::Vertex
            ? SDL_SHADERCROSS_SHADERSTAGE_VERTEX
            : SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT;

    std::filesystem::path include_dir = shader_dir_;
    if (include_dir.empty() && source.path.has_parent_path())
    {
        include_dir = source.path.parent_path();
    }

    const std::string include_dir_str = include_dir.string();

    SDL_ShaderCross_HLSL_Info hlsl_info {};
    hlsl_info.source     = content_result->c_str();
    hlsl_info.entrypoint = source.entry_point.c_str();
    hlsl_info.include_dir =
        include_dir_str.empty() ? nullptr : include_dir_str.c_str();
    hlsl_info.defines      = nullptr;
    hlsl_info.shader_stage = stage;
    hlsl_info.props        = 0;

    std::size_t spirv_size {};
    void* spirv = SDL_ShaderCross_CompileSPIRVFromHLSL(&hlsl_info, &spirv_size);
    if (spirv == nullptr)
    {
        return std::unexpected(
            std::format("Failed to compile SPIRV from '{}': {}",
                        source.path.string(),
                        SDL_GetError()));
    }

    SDL_ShaderCross_SPIRV_Info spirv_info {};
    spirv_info.bytecode      = static_cast<Uint8*>(spirv);
    spirv_info.bytecode_size = spirv_size;
    spirv_info.entrypoint    = source.entry_point.c_str();
    spirv_info.shader_stage  = stage;
    spirv_info.props         = 0;

    SDL_ShaderCross_GraphicsShaderMetadata* metadata =
        SDL_ShaderCross_ReflectGraphicsSPIRV(
            spirv_info.bytecode, spirv_info.bytecode_size, 0);
    if (metadata == nullptr)
    {
        SDL_free(spirv);
        return std::unexpected(
            std::format("Failed to reflect SPIRV from '{}': {}",
                        source.path.string(),
                        SDL_GetError()));
    }

    SDL_GPUShader* shader = SDL_ShaderCross_CompileGraphicsShaderFromSPIRV(
        device_, &spirv_info, &metadata->resource_info, 0);

    SDL_free(metadata);
    SDL_free(spirv);

    if (shader == nullptr)
    {
        return std::unexpected(
            std::format("Failed to create GPU shader from '{}': {}",
                        source.path.string(),
                        SDL_GetError()));
    }

    spdlog::debug("Compiled shader: {}", source.path.string());
    return shader;
}

std::expected<ShaderProgram*, std::string> ShaderManager::load_program(
    const ShaderProgramDesc& desc)
{
    std::string name(desc.name);

    if (auto it = programs_.find(name); it != programs_.end())
    {
        spdlog::warn("Shader program '{}' already loaded, returning existing",
                     name);
        return &it->second;
    }

    auto vertex_result = compile_shader(desc.vertex);
    if (!vertex_result)
    {
        return std::unexpected(vertex_result.error());
    }

    auto fragment_result = compile_shader(desc.fragment);
    if (!fragment_result)
    {
        SDL_ReleaseGPUShader(device_, *vertex_result);
        return std::unexpected(fragment_result.error());
    }

    ShaderProgram program;
    program.vertex_shader_     = *vertex_result;
    program.fragment_shader_   = *fragment_result;
    program.device_            = device_;
    program.vertex_source_     = desc.vertex;
    program.fragment_source_   = desc.fragment;
    program.name_              = name;
    program.vertex_mod_time_   = get_mod_time(desc.vertex.path);
    program.fragment_mod_time_ = get_mod_time(desc.fragment.path);

    auto [it, inserted] = programs_.emplace(name, std::move(program));

    spdlog::info("Loaded shader program: {}", name);
    return &it->second;
}

ShaderProgram* ShaderManager::get_program(std::string_view name) noexcept
{
    if (auto it = programs_.find(std::string(name)); it != programs_.end())
    {
        return &it->second;
    }
    return nullptr;
}

bool ShaderManager::reload_program(ShaderProgram& program)
{
    auto vertex_result = compile_shader(program.vertex_source_);
    if (!vertex_result)
    {
        spdlog::error("Failed to reload vertex shader for '{}': {}",
                      program.name_,
                      vertex_result.error());
        return false;
    }

    auto fragment_result = compile_shader(program.fragment_source_);
    if (!fragment_result)
    {
        SDL_ReleaseGPUShader(device_, *vertex_result);
        spdlog::error("Failed to reload fragment shader for '{}': {}",
                      program.name_,
                      fragment_result.error());
        return false;
    }

    // Release old shaders
    if (program.vertex_shader_ != nullptr)
    {
        SDL_ReleaseGPUShader(device_, program.vertex_shader_);
    }
    if (program.fragment_shader_ != nullptr)
    {
        SDL_ReleaseGPUShader(device_, program.fragment_shader_);
    }

    // Assign new shaders
    program.vertex_shader_     = *vertex_result;
    program.fragment_shader_   = *fragment_result;
    program.vertex_mod_time_   = get_mod_time(program.vertex_source_.path);
    program.fragment_mod_time_ = get_mod_time(program.fragment_source_.path);

    spdlog::info("Reloaded shader program: {}", program.name_);
    return true;
}

void ShaderManager::check_for_updates()
{
    if (!hot_reload_enabled_)
    {
        return;
    }

    for (auto& [name, program] : programs_)
    {
        const SDL_Time vertex_time = get_mod_time(program.vertex_source_.path);
        const SDL_Time fragment_time =
            get_mod_time(program.fragment_source_.path);

        const bool vertex_changed = vertex_time > program.vertex_mod_time_;
        const bool fragment_changed =
            fragment_time > program.fragment_mod_time_;

        if (vertex_changed || fragment_changed)
        {
            spdlog::debug("Shader change detected for program '{}' (vertex={}, "
                          "fragment={})",
                          name,
                          vertex_changed,
                          fragment_changed);

            if (reload_program(program))
            {
                if (reload_callback_)
                {
                    reload_callback_(name);
                }
            }
        }
    }
}

void ShaderManager::release_all() noexcept
{
    for (auto& [name, program] : programs_)
    {
        program.release();
    }
    programs_.clear();
}

} // namespace euengine
