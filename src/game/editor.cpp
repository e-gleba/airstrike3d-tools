#include "editor.hpp"
#include "camera.hpp"
#include "game_api.hpp"

#include <imgui.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstdio>

namespace as3
{

namespace
{

std::vector<vertex> create_bounds_vertices(const glm::vec3& color)
{
    constexpr glm::vec3 corners[8] = {
        { -0.5f, -0.5f, -0.5f }, { 0.5f, -0.5f, -0.5f },
        { 0.5f,  0.5f, -0.5f }, { -0.5f,  0.5f, -0.5f },
        { -0.5f, -0.5f,  0.5f }, { 0.5f, -0.5f,  0.5f },
        { 0.5f,  0.5f,  0.5f }, { -0.5f,  0.5f,  0.5f },
    };

    std::vector<vertex> verts;
    for (const auto& c : corners)
        verts.push_back({ c, color });

    return verts;
}

constexpr uint16_t bounds_indices[24] = {
    0, 1, 1, 2, 2, 3, 3, 0,
    4, 5, 5, 6, 6, 7, 7, 4,
    0, 4, 1, 5, 2, 6, 3, 7,
};

std::vector<vertex> create_axis_vertices()
{
    return {
        { { 0.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } },
        { { 1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } },
        { { 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f } },
        { { 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f } },
        { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } },
        { { 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } },
    };
}

constexpr uint16_t axis_indices[6] = { 0, 1, 2, 3, 4, 5 };

// Get file extension
std::string get_extension(const std::string& path)
{
    auto pos = path.rfind('.');
    if (pos != std::string::npos)
        return path.substr(pos);
    return "";
}

// Get filename from path
std::string get_filename(const std::string& path)
{
    auto pos = path.find_last_of("/\\");
    if (pos != std::string::npos)
        return path.substr(pos + 1);
    return path;
}

} // namespace

void Editor::init(engine_context* ctx)
{
    ctx_ = ctx;
    
    auto bounds_verts = create_bounds_vertices(config_.bounds_color);
    bounds_mesh_ = ctx_->renderer->create_mesh(
        bounds_verts, 
        std::span(bounds_indices, 24),
        primitive_type::lines
    );

    auto axis_verts = create_axis_vertices();
    axis_mesh_ = ctx_->renderer->create_mesh(
        axis_verts,
        std::span(axis_indices, 6),
        primitive_type::lines
    );

    spdlog::info("Editor initialized");
}

void Editor::shutdown()
{
    if (bounds_mesh_ != invalid_mesh)
        ctx_->renderer->destroy_mesh(bounds_mesh_);
    if (axis_mesh_ != invalid_mesh)
        ctx_->renderer->destroy_mesh(axis_mesh_);

    bounds_mesh_ = invalid_mesh;
    axis_mesh_   = invalid_mesh;
    objects_.clear();
    ctx_ = nullptr;
}

void Editor::register_composite(const std::string& name, CompositeModel* model, 
                                transform* xform, const std::string& config_path)
{
    objects_.push_back({
        .name        = name,
        .config_path = config_path,
        .composite   = model,
        .model       = invalid_model,
        .xform       = xform,
    });
}

void Editor::register_model(const std::string& name, model_handle model, transform* xform)
{
    objects_.push_back({
        .name        = name,
        .config_path = "",
        .composite   = nullptr,
        .model       = model,
        .xform       = xform,
    });
}

void Editor::clear_objects()
{
    objects_.clear();
    selected_idx_ = -1;
    selected_attachment_ = -1;
}

void Editor::update([[maybe_unused]] float dt)
{
}

void Editor::draw_gizmos()
{
    if (!visible_ || !ctx_)
        return;

    if (config_.show_bounds)
    {
        for (size_t i = 0; i < objects_.size(); ++i)
        {
            const auto& obj = objects_[i];
            if (!obj.xform)
                continue;

            bounds b;
            if (obj.composite && obj.composite->is_loaded())
                b = obj.composite->get_bounds();
            else if (obj.model != invalid_model)
                b = ctx_->renderer->get_bounds(obj.model);
            else
                continue;

            const auto& color = (static_cast<int>(i) == selected_idx_) 
                ? config_.selected_color 
                : config_.bounds_color;

            draw_bounds_gizmo(b, *obj.xform, color);
        }
    }

    if (config_.show_axes && selected_idx_ >= 0 && 
        selected_idx_ < static_cast<int>(objects_.size()))
    {
        const auto& obj = objects_[static_cast<size_t>(selected_idx_)];
        if (obj.xform)
            draw_axis_gizmo(obj.xform->position, config_.gizmo_scale * 2.0f);
    }
}

void Editor::draw_bounds_gizmo(const bounds& b, const transform& xform, const glm::vec3& color)
{
    glm::vec3 center = b.center();
    glm::vec3 size   = b.size();
    glm::vec3 scaled_center = xform.position + center * xform.scale;
    glm::vec3 scaled_size   = size * xform.scale;

    auto verts = create_bounds_vertices(color);
    for (auto& v : verts)
        v.position = scaled_center + v.position * scaled_size;

    auto mesh = ctx_->renderer->create_mesh(verts, std::span(bounds_indices, 24), primitive_type::lines);
    ctx_->renderer->draw(mesh);
    ctx_->renderer->destroy_mesh(mesh);
}

void Editor::draw_axis_gizmo(const glm::vec3& pos, float size)
{
    auto verts = create_axis_vertices();
    for (auto& v : verts)
        v.position = pos + v.position * size;

    auto mesh = ctx_->renderer->create_mesh(verts, std::span(axis_indices, 6), primitive_type::lines);
    ctx_->renderer->draw(mesh);
    ctx_->renderer->destroy_mesh(mesh);
}

void Editor::draw_ui()
{
    if (!visible_)
        return;

    ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx_->imgui_ctx));
    draw_main_window();
    draw_file_browser_modal();
}

void Editor::draw_main_window()
{
    ImGui::SetNextWindowSize(ImVec2(450, 600), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 470, 20), ImGuiCond_FirstUseEver);

    ImGuiWindowFlags flags = ImGuiWindowFlags_MenuBar;
    if (!ImGui::Begin("Scene Editor", &visible_, flags))
    {
        ImGui::End();
        return;
    }

    // Menu bar
    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Save All Configs"))
            {
                for (auto& obj : objects_)
                {
                    if (obj.composite && !obj.config_path.empty())
                        obj.composite->save(obj.config_path);
                }
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View"))
        {
            ImGui::Checkbox("Bounding Boxes", &config_.show_bounds);
            ImGui::Checkbox("Axes Gizmo", &config_.show_axes);
            ImGui::SliderFloat("Gizmo Size", &config_.gizmo_scale, 0.5f, 5.0f);
            ImGui::Separator();
            ImGui::ColorEdit3("Bounds Color", &config_.bounds_color.x);
            ImGui::ColorEdit3("Selection Color", &config_.selected_color.x);
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    // Two-column layout
    float panel_width = ImGui::GetContentRegionAvail().x;
    
    // Hierarchy panel (top)
    ImGui::BeginChild("##hierarchy", ImVec2(panel_width, 150), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeY);
    draw_hierarchy_panel();
    ImGui::EndChild();

    // Inspector panel (bottom)
    ImGui::BeginChild("##inspector", ImVec2(0, 0), ImGuiChildFlags_Borders);
    draw_inspector_panel();
    ImGui::EndChild();

    ImGui::End();
}

void Editor::draw_hierarchy_panel()
{
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "HIERARCHY");
    ImGui::Separator();

    for (size_t i = 0; i < objects_.size(); ++i)
    {
        auto& obj = objects_[i];
        bool selected = (static_cast<int>(i) == selected_idx_);

        ImGuiTreeNodeFlags node_flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (selected)
            node_flags |= ImGuiTreeNodeFlags_Selected;

        // Type indicator
        std::string label = std::string(obj.composite ? "[Composite] " : "[Model] ") + obj.name;

        ImGui::PushID(static_cast<int>(i));
        if (ImGui::TreeNodeEx(label.c_str(), node_flags))
        {
            if (ImGui::IsItemClicked())
            {
                selected_idx_ = static_cast<int>(i);
                selected_attachment_ = -1;
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
}

void Editor::draw_inspector_panel()
{
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "INSPECTOR");
    ImGui::Separator();

    if (selected_idx_ < 0 || selected_idx_ >= static_cast<int>(objects_.size()))
    {
        ImGui::TextDisabled("Select an object from hierarchy");
        return;
    }

    auto& obj = objects_[static_cast<size_t>(selected_idx_)];

    // Object header
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "%s", obj.name.c_str());
    if (!obj.config_path.empty())
    {
        ImGui::SameLine();
        ImGui::TextDisabled("(%s)", obj.config_path.c_str());
    }
    ImGui::Separator();

    // Transform section
    if (obj.xform && ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Indent();
        ImGui::DragFloat3("Position", &obj.xform->position.x, 0.1f);
        ImGui::DragFloat3("Rotation", &obj.xform->rotation.x, 1.0f);
        ImGui::DragFloat3("Scale", &obj.xform->scale.x, 0.01f, 0.001f, 100.0f);
        
        if (ImGui::Button("Reset"))
        {
            obj.xform->position = glm::vec3(0.0f);
            obj.xform->rotation = glm::vec3(0.0f);
            obj.xform->scale    = glm::vec3(1.0f);
        }
        ImGui::Unindent();
    }

    // Bounds info
    bounds b;
    if (obj.composite && obj.composite->is_loaded())
        b = obj.composite->get_bounds();
    else if (obj.model != invalid_model)
        b = ctx_->renderer->get_bounds(obj.model);

    if (ImGui::CollapsingHeader("Bounds"))
    {
        ImGui::Indent();
        ImGui::Text("Min: %.2f, %.2f, %.2f", b.min.x, b.min.y, b.min.z);
        ImGui::Text("Max: %.2f, %.2f, %.2f", b.max.x, b.max.y, b.max.z);
        ImGui::Text("Size: %.2f x %.2f x %.2f", b.size().x, b.size().y, b.size().z);
        ImGui::Unindent();
    }

    // Composite model section
    if (obj.composite && obj.composite->is_loaded())
    {
        if (ImGui::CollapsingHeader("Model", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Indent();
            ImGui::Text("Body: %s", get_filename(obj.composite->config().body_path).c_str());
            
            if (ImGui::Button("Change Model..."))
            {
                open_file_browser("Select Model", "assets/models", {".obj"}, 
                    [&obj](const std::string& path) {
                        obj.composite->config().body_path = path;
                        // Reload would need to happen here
                    });
            }
            ImGui::Unindent();
        }

        if (ImGui::CollapsingHeader("Attachments", ImGuiTreeNodeFlags_DefaultOpen))
        {
            draw_attachment_editor();
        }

        // Save button
        ImGui::Separator();
        if (!obj.config_path.empty())
        {
            if (ImGui::Button("Save Config", ImVec2(-1, 0)))
            {
                obj.composite->save(obj.config_path);
                spdlog::info("Saved: {}", obj.config_path);
            }
        }
        else
        {
            if (ImGui::Button("Save As...", ImVec2(-1, 0)))
            {
                std::string default_path = "assets/configs/helicopters/" + obj.name + ".yaml";
                for (auto& c : default_path)
                {
                    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + 32);
                    if (c == ' ') c = '_';
                }
                obj.config_path = default_path;
                obj.composite->save(obj.config_path);
            }
        }
    }
}

void Editor::draw_attachment_editor()
{
    auto& obj = objects_[static_cast<size_t>(selected_idx_)];
    auto* comp = obj.composite;
    if (!comp)
        return;

    auto& attachments = comp->attachments();

    ImGui::Indent();

    // List attachments
    for (size_t i = 0; i < attachments.size(); ++i)
    {
        auto& att = attachments[i];
        bool selected = (static_cast<int>(i) == selected_attachment_);

        std::string header = att.config.name.empty() 
            ? ("Attachment " + std::to_string(i)) 
            : att.config.name;
        
        if (att.config.rotation_speed > 0)
            header += " [rotating]";

        ImGui::PushID(static_cast<int>(i));

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_AllowOverlap;
        if (selected)
            flags |= ImGuiTreeNodeFlags_Selected;

        bool open = ImGui::TreeNodeEx(header.c_str(), flags);
        
        if (ImGui::IsItemClicked())
            selected_attachment_ = static_cast<int>(i);

        // Delete button on same line
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 20);
        if (ImGui::SmallButton("X"))
        {
            comp->config().attachments.erase(comp->config().attachments.begin() + static_cast<long>(i));
            comp->reload_attachments(ctx_->renderer);
            selected_attachment_ = -1;
            ImGui::PopID();
            if (open) ImGui::TreePop();
            break;
        }

        if (open)
        {
            // Name
            char name_buf[128] = {};
            std::snprintf(name_buf, sizeof(name_buf), "%s", att.config.name.c_str());
            if (ImGui::InputText("Name", name_buf, sizeof(name_buf)))
                att.config.name = name_buf;

            // Model path with browse button
            ImGui::Text("Model: %s", get_filename(att.config.model_path).c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton("Browse##model"))
            {
                size_t idx = i;
                open_file_browser("Select Rotor Model", "assets/models/helics/vints", {".obj"},
                    [this, idx](const std::string& path) {
                        auto& obj2 = objects_[static_cast<size_t>(selected_idx_)];
                        if (obj2.composite && idx < obj2.composite->attachments().size())
                        {
                            obj2.composite->attachments()[idx].config.model_path = path;
                            obj2.composite->config().attachments[idx].model_path = path;
                            obj2.composite->reload_attachments(ctx_->renderer);
                        }
                    });
            }

            // Texture path with browse button
            ImGui::Text("Texture: %s", att.config.texture_path.empty() ? "(auto)" : get_filename(att.config.texture_path).c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton("Browse##tex"))
            {
                size_t idx = i;
                open_file_browser("Select Texture", "assets/models/helics/vints", {".tga", ".png"},
                    [this, idx](const std::string& path) {
                        auto& obj2 = objects_[static_cast<size_t>(selected_idx_)];
                        if (obj2.composite && idx < obj2.composite->attachments().size())
                        {
                            obj2.composite->attachments()[idx].config.texture_path = path;
                            obj2.composite->config().attachments[idx].texture_path = path;
                        }
                    });
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Clear##tex"))
            {
                att.config.texture_path.clear();
                comp->config().attachments[i].texture_path.clear();
            }

            ImGui::Separator();

            // Transform
            ImGui::DragFloat3("Offset", &att.config.offset.x, 0.05f, -50.0f, 50.0f);
            
            // Rotation axis with quick buttons
            ImGui::DragFloat3("Rot Axis", &att.config.rotation_axis.x, 0.1f, -1.0f, 1.0f);
            ImGui::SameLine();
            if (ImGui::SmallButton("X")) att.config.rotation_axis = { 1, 0, 0 };
            ImGui::SameLine();
            if (ImGui::SmallButton("Y")) att.config.rotation_axis = { 0, 1, 0 };
            ImGui::SameLine();
            if (ImGui::SmallButton("Z")) att.config.rotation_axis = { 0, 0, 1 };

            ImGui::DragFloat("Speed", &att.config.rotation_speed, 10.0f, 0.0f, 3600.0f, "%.0f deg/s");
            ImGui::DragFloat("Scale", &att.config.scale, 0.02f, 0.01f, 5.0f);

            // Sync to config
            if (i < comp->config().attachments.size())
            {
                comp->config().attachments[i].offset = att.config.offset;
                comp->config().attachments[i].rotation_axis = att.config.rotation_axis;
                comp->config().attachments[i].rotation_speed = att.config.rotation_speed;
                comp->config().attachments[i].scale = att.config.scale;
                comp->config().attachments[i].name = att.config.name;
            }

            ImGui::TreePop();
        }

        ImGui::PopID();
    }

    // Add button
    if (ImGui::Button("+ Add Attachment", ImVec2(-1, 0)))
    {
        attachment_config new_att;
        new_att.name           = "new_rotor";
        new_att.model_path     = "assets/models/helics/vints/vint_a.obj";
        new_att.offset         = { 0.0f, 2.0f, 0.0f };
        new_att.rotation_axis  = { 0.0f, 1.0f, 0.0f };
        new_att.rotation_speed = 720.0f;
        new_att.scale          = 1.0f;

        comp->config().attachments.push_back(new_att);
        comp->reload_attachments(ctx_->renderer);
        selected_attachment_ = static_cast<int>(attachments.size()) - 1;
    }

    ImGui::Unindent();
}

void Editor::open_file_browser(const std::string& title, const std::string& start_path,
                               const std::vector<std::string>& extensions,
                               std::function<void(const std::string&)> callback)
{
    file_browser_.open = true;
    file_browser_.title = title;
    file_browser_.current_path = start_path;
    file_browser_.selected_file.clear();
    file_browser_.extensions = extensions;
    file_browser_.on_select = std::move(callback);
    file_browser_.needs_refresh = true;
}

void Editor::refresh_file_browser()
{
    file_browser_.directories.clear();
    file_browser_.files.clear();

    try
    {
        for (const auto& entry : std::filesystem::directory_iterator(file_browser_.current_path))
        {
            std::string name = entry.path().filename().string();
            if (name[0] == '.') continue;  // Skip hidden

            if (entry.is_directory())
            {
                file_browser_.directories.push_back(name);
            }
            else if (entry.is_regular_file())
            {
                std::string ext = entry.path().extension().string();
                // Convert to lowercase
                for (auto& c : ext)
                    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + 32);

                bool match = file_browser_.extensions.empty();
                for (const auto& filter_ext : file_browser_.extensions)
                {
                    std::string filter_lower = filter_ext;
                    for (auto& c : filter_lower)
                        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + 32);
                    if (ext == filter_lower)
                    {
                        match = true;
                        break;
                    }
                }
                if (match)
                    file_browser_.files.push_back(name);
            }
        }

        std::sort(file_browser_.directories.begin(), file_browser_.directories.end());
        std::sort(file_browser_.files.begin(), file_browser_.files.end());
    }
    catch (const std::exception& e)
    {
        spdlog::error("File browser error: {}", e.what());
    }

    file_browser_.needs_refresh = false;
}

void Editor::draw_file_browser_modal()
{
    if (!file_browser_.open)
        return;

    ImGui::OpenPopup(file_browser_.title.c_str());

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);

    if (ImGui::BeginPopupModal(file_browser_.title.c_str(), &file_browser_.open))
    {
        if (file_browser_.needs_refresh)
            refresh_file_browser();

        // Path bar
        ImGui::Text("Path: %s", file_browser_.current_path.c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("Up"))
        {
            std::filesystem::path p(file_browser_.current_path);
            if (p.has_parent_path() && p != p.root_path())
            {
                file_browser_.current_path = p.parent_path().string();
                file_browser_.needs_refresh = true;
            }
        }

        ImGui::Separator();

        // File list
        ImGui::BeginChild("##files", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * 2), ImGuiChildFlags_Borders);

        // Directories first
        for (const auto& dir : file_browser_.directories)
        {
            std::string label = "[DIR] " + dir;
            if (ImGui::Selectable(label.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick))
            {
                if (ImGui::IsMouseDoubleClicked(0))
                {
                    file_browser_.current_path += "/" + dir;
                    file_browser_.needs_refresh = true;
                }
            }
        }

        // Files
        for (const auto& file : file_browser_.files)
        {
            bool selected = (file_browser_.selected_file == file);
            if (ImGui::Selectable(file.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick))
            {
                file_browser_.selected_file = file;
                if (ImGui::IsMouseDoubleClicked(0))
                {
                    std::string full_path = file_browser_.current_path + "/" + file;
                    if (file_browser_.on_select)
                        file_browser_.on_select(full_path);
                    file_browser_.open = false;
                }
            }
        }

        ImGui::EndChild();

        // Selected file display
        ImGui::Text("Selected: %s", file_browser_.selected_file.empty() ? "(none)" : file_browser_.selected_file.c_str());

        // Buttons
        if (ImGui::Button("Select", ImVec2(120, 0)))
        {
            if (!file_browser_.selected_file.empty())
            {
                std::string full_path = file_browser_.current_path + "/" + file_browser_.selected_file;
                if (file_browser_.on_select)
                    file_browser_.on_select(full_path);
                file_browser_.open = false;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0)))
        {
            file_browser_.open = false;
        }

        ImGui::EndPopup();
    }
}

} // namespace as3
