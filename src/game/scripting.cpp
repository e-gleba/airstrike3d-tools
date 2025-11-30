#include "scripting.hpp"

#include <imgui.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <format>

namespace as3
{

ScriptManager::ScriptManager()
{
    lua_.open_libraries(
        sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table);
}

ScriptManager::~ScriptManager()
{
    shutdown();
}

void ScriptManager::init(IRenderer* renderer)
{
    renderer_ = renderer;
    setup_lua_api();
    log_message(script_log_entry::level::info, "Lua scripting initialized");
}

void ScriptManager::shutdown()
{
    renderer_ = nullptr;
}

void ScriptManager::setup_lua_api()
{
    // Vec3 type
    lua_.new_usertype<glm::vec3>(
        "vec3",
        sol::constructors<glm::vec3(),
                          glm::vec3(float),
                          glm::vec3(float, float, float)>(),
        "x",
        &glm::vec3::x,
        "y",
        &glm::vec3::y,
        "z",
        &glm::vec3::z,
        sol::meta_function::addition,
        [](const glm::vec3& a, const glm::vec3& b) { return a + b; },
        sol::meta_function::subtraction,
        [](const glm::vec3& a, const glm::vec3& b) { return a - b; },
        sol::meta_function::multiplication,
        [](const glm::vec3& a, float b) { return a * b; });

    // Object part
    lua_.new_usertype<object_part>("Part",
                                   sol::constructors<object_part()>(),
                                   "name",
                                   &object_part::name,
                                   "model_path",
                                   &object_part::model_path,
                                   "offset",
                                   &object_part::offset,
                                   "rotation",
                                   &object_part::rotation,
                                   "scale",
                                   &object_part::scale,
                                   "rotation_axis",
                                   &object_part::rotation_axis,
                                   "rotation_speed",
                                   &object_part::rotation_speed,
                                   "min_angle",
                                   &object_part::min_angle,
                                   "max_angle",
                                   &object_part::max_angle,
                                   "can_rotate",
                                   &object_part::can_rotate,
                                   "continuous",
                                   &object_part::continuous,
                                   "target_angle",
                                   &object_part::target_angle,
                                   "current_angle",
                                   &object_part::current_angle,
                                   "parent_index",
                                   &object_part::parent_index);

    // Compound object
    lua_.new_usertype<compound_object>(
        "Object",
        "name",
        &compound_object::name,
        "position",
        &compound_object::position,
        "rotation",
        &compound_object::rotation,
        "scale",
        &compound_object::scale,
        "selected",
        &compound_object::selected,
        "selection_radius",
        &compound_object::selection_radius,
        "move_speed",
        &compound_object::move_speed,
        "turn_speed",
        &compound_object::turn_speed,
        "target_pos",
        &compound_object::target_pos,
        "has_target",
        &compound_object::has_target,
        "can_move",
        &compound_object::can_move,
        "current_speed",
        &compound_object::current_speed,
        "parts",
        &compound_object::parts,
        "add_part",
        [](compound_object& obj, const object_part& part)
        { obj.parts.push_back(part); },
        "get_part",
        [](compound_object& obj, const std::string& name) -> object_part*
        {
            for (auto& part : obj.parts)
                if (part.name == name)
                    return &part;
            return nullptr;
        });

    // Logging functions
    lua_["print"] = [this](const std::string& msg)
    { log_message(script_log_entry::level::info, msg, "script"); };

    lua_["warn"] = [this](const std::string& msg)
    { log_message(script_log_entry::level::warning, msg, "script"); };

    lua_["error"] = [this](const std::string& msg)
    { log_message(script_log_entry::level::error, msg, "script"); };

    // Math helpers
    lua_["lerp"]  = [](float a, float b, float t) { return a + (b - a) * t; };
    lua_["clamp"] = [](float v, float min, float max)
    { return v < min ? min : (v > max ? max : v); };
    lua_["normalize_angle"] = [](float angle)
    {
        while (angle > 180.0f)
            angle -= 360.0f;
        while (angle < -180.0f)
            angle += 360.0f;
        return angle;
    };
    lua_["angle_diff"] = [](float from, float to)
    {
        float diff = to - from;
        while (diff > 180.0f)
            diff -= 360.0f;
        while (diff < -180.0f)
            diff += 360.0f;
        return diff;
    };
}

bool ScriptManager::load_object(compound_object&             obj,
                                const std::filesystem::path& script)
{
    if (!std::filesystem::exists(script))
    {
        log_message(script_log_entry::level::error,
                    std::format("Script not found: {}", script.string()));
        return false;
    }

    obj.script_path = script.string();
    obj.state_key   = std::format(
        "obj_{}", std::chrono::steady_clock::now().time_since_epoch().count());

    try
    {
        // Create object state table
        lua_[obj.state_key] = lua_.create_table();

        // Load and execute script
        auto result =
            lua_.safe_script_file(script.string(), sol::script_pass_on_error);
        if (!result.valid())
        {
            sol::error err = result;
            handle_lua_error(err, "load");
            return false;
        }

        // Call define() to get object definition
        sol::protected_function define_fn = lua_["define"];
        if (!define_fn.valid())
        {
            log_message(script_log_entry::level::error,
                        "Script missing 'define()' function",
                        script.filename().string());
            return false;
        }

        auto define_result = define_fn(obj);
        if (!define_result.valid())
        {
            sol::error err = define_result;
            handle_lua_error(err, "define");
            return false;
        }

        // Load models for all parts
        for (auto& part : obj.parts)
        {
            if (!part.model_path.empty() && renderer_)
            {
                part.model =
                    renderer_->load_model(part.model_path, glm::vec3(1.0f));
                if (part.model == invalid_model)
                {
                    log_message(script_log_entry::level::warning,
                                std::format("Failed to load model: {}",
                                            part.model_path));
                }
            }
        }

        // Call init() if exists
        sol::protected_function init_fn = lua_["init"];
        if (init_fn.valid())
        {
            auto init_result = init_fn(obj, lua_[obj.state_key]);
            if (!init_result.valid())
            {
                sol::error err = init_result;
                handle_lua_error(err, "init");
            }
        }

        log_message(
            script_log_entry::level::info,
            std::format("Loaded: {} ({} parts)", obj.name, obj.parts.size()),
            script.filename().string());

        return true;
    }
    catch (const sol::error& e)
    {
        handle_lua_error(e, "load");
        return false;
    }
}

bool ScriptManager::reload_object(compound_object& obj)
{
    if (obj.script_path.empty())
        return false;

    // Unload old models
    if (renderer_)
    {
        for (auto& part : obj.parts)
        {
            if (part.model != invalid_model)
            {
                renderer_->unload_model(part.model);
                part.model = invalid_model;
            }
        }
    }

    obj.parts.clear();
    return load_object(obj, obj.script_path);
}

void ScriptManager::update_object(compound_object& obj, float dt)
{
    if (obj.script_path.empty())
        return;

    try
    {
        // Call update() if exists
        sol::protected_function update_fn = lua_["update"];
        if (update_fn.valid())
        {
            auto result = update_fn(obj, dt, lua_[obj.state_key]);
            if (!result.valid())
            {
                sol::error err = result;
                handle_lua_error(err, "update");
            }
        }

        // Update part rotations
        for (auto& part : obj.parts)
        {
            if (!part.can_rotate)
                continue;

            if (part.continuous)
            {
                // Continuous rotation (rotor)
                part.current_angle += part.rotation_speed * dt;
                while (part.current_angle > 360.0f)
                    part.current_angle -= 360.0f;
            }
            else
            {
                // Move toward target angle
                float diff = part.target_angle - part.current_angle;
                while (diff > 180.0f)
                    diff -= 360.0f;
                while (diff < -180.0f)
                    diff += 360.0f;

                float max_delta = part.rotation_speed * dt;
                if (std::abs(diff) <= max_delta)
                {
                    part.current_angle = part.target_angle;
                }
                else
                {
                    part.current_angle += (diff > 0 ? max_delta : -max_delta);
                }

                // Clamp to limits
                part.current_angle = std::clamp(
                    part.current_angle, part.min_angle, part.max_angle);
            }

            // Apply rotation to part's rotation vector based on axis
            part.rotation = part.rotation_axis * part.current_angle;
        }
    }
    catch (const sol::error& e)
    {
        handle_lua_error(e, "update");
    }
}

void ScriptManager::on_select(compound_object& obj)
{
    if (obj.script_path.empty())
        return;

    try
    {
        sol::protected_function fn = lua_["on_select"];
        if (fn.valid())
        {
            auto result = fn(obj, lua_[obj.state_key]);
            if (!result.valid())
            {
                sol::error err = result;
                handle_lua_error(err, "on_select");
            }
        }
    }
    catch (const sol::error& e)
    {
        handle_lua_error(e, "on_select");
    }
}

void ScriptManager::on_deselect(compound_object& obj)
{
    if (obj.script_path.empty())
        return;

    try
    {
        sol::protected_function fn = lua_["on_deselect"];
        if (fn.valid())
        {
            auto result = fn(obj, lua_[obj.state_key]);
            if (!result.valid())
            {
                sol::error err = result;
                handle_lua_error(err, "on_deselect");
            }
        }
    }
    catch (const sol::error& e)
    {
        handle_lua_error(e, "on_deselect");
    }
}

void ScriptManager::on_move_command(compound_object& obj,
                                    const glm::vec3& target)
{
    obj.target_pos = target;
    obj.has_target = true;

    if (obj.script_path.empty())
        return;

    try
    {
        sol::protected_function fn = lua_["on_move"];
        if (fn.valid())
        {
            auto result = fn(obj, target, lua_[obj.state_key]);
            if (!result.valid())
            {
                sol::error err = result;
                handle_lua_error(err, "on_move");
            }
        }
    }
    catch (const sol::error& e)
    {
        handle_lua_error(e, "on_move");
    }
}

void ScriptManager::log_message(script_log_entry::level lvl,
                                const std::string&      msg,
                                const std::string&      src)
{
    if (lvl == script_log_entry::level::error)
        ++error_count_;

    log_.push_back({ lvl, msg, src });

    // Keep log bounded
    while (log_.size() > k_max_log_entries)
        log_.pop_front();

    // Also log to spdlog
    switch (lvl)
    {
        case script_log_entry::level::info:
            spdlog::info("[Lua] {}", msg);
            break;
        case script_log_entry::level::warning:
            spdlog::warn("[Lua] {}", msg);
            break;
        case script_log_entry::level::error:
            spdlog::error("[Lua] {}", msg);
            break;
    }
}

void ScriptManager::handle_lua_error(const sol::error&  e,
                                     const std::string& context)
{
    log_message(script_log_entry::level::error,
                std::format("[{}] {}", context, e.what()));
}

void ScriptManager::draw_log_window(bool* open)
{
    if (!*open)
        return;

    ImGui::SetNextWindowSize(ImVec2(500, 300), ImGuiCond_FirstUseEver);

    ImGuiWindowFlags flags = ImGuiWindowFlags_None;
    if (error_count_ > 0)
    {
        // Flash red border if errors
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2.0f);
    }

    if (ImGui::Begin("Script Log", open, flags))
    {
        // Toolbar
        if (ImGui::Button("Clear"))
        {
            clear_log();
            error_count_ = 0;
        }
        ImGui::SameLine();
        if (error_count_ > 0)
        {
            ImGui::TextColored(
                ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Errors: %d", error_count_);
        }
        else
        {
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "No errors");
        }

        ImGui::Separator();

        // Log entries
        ImGui::BeginChild("LogScroll",
                          ImVec2(0, 0),
                          false,
                          ImGuiWindowFlags_HorizontalScrollbar);

        for (const auto& entry : log_)
        {
            ImVec4      color;
            const char* prefix;
            switch (entry.type)
            {
                case script_log_entry::level::info:
                    color  = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
                    prefix = "[INFO]";
                    break;
                case script_log_entry::level::warning:
                    color  = ImVec4(1.0f, 0.8f, 0.3f, 1.0f);
                    prefix = "[WARN]";
                    break;
                case script_log_entry::level::error:
                    color  = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
                    prefix = "[ERROR]";
                    break;
            }

            ImGui::TextColored(color, "%s", prefix);
            ImGui::SameLine();
            if (!entry.source.empty())
            {
                ImGui::TextColored(ImVec4(0.5f, 0.7f, 1.0f, 1.0f),
                                   "[%s]",
                                   entry.source.c_str());
                ImGui::SameLine();
            }
            ImGui::TextWrapped("%s", entry.message.c_str());
        }

        // Auto-scroll
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);

        ImGui::EndChild();
    }
    ImGui::End();

    if (error_count_ > 0)
    {
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }
}

} // namespace as3
