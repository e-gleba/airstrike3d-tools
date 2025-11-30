#include "editor.hpp"
#include "game_api.hpp"

#include <imgui.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstring>

namespace as3
{

static const char* unit_type_names[] = {
    "Static", "Ground Vehicle", "Helicopter", "Aircraft"
};

static const char* move_state_names[] = {
    "Idle", "Rotating", "Moving", "Hovering"
};

void Editor::init(engine_context* ctx)
{
    ctx_ = ctx;
    scene_.init(ctx->renderer);

    scene_list_ = Scene::list_scenes("assets/scenes");

    if (ctx->audio)
    {
        music_files_ = ctx->audio->list_music_files();
        sound_files_ = ctx->audio->list_sound_files();
    }

    spdlog::info("Editor initialized ({} scenes, {} music, {} sounds)",
                 scene_list_.size(),
                 music_files_.size(),
                 sound_files_.size());
}

void Editor::shutdown()
{
    scene_.shutdown();

    if (ctx_ && ctx_->audio)
    {
        for (auto h : loaded_music_)
            if (h != invalid_music)
                ctx_->audio->unload_music(h);
        for (auto h : loaded_sounds_)
            if (h != invalid_sound)
                ctx_->audio->unload_sound(h);
    }
    loaded_music_.clear();
    loaded_sounds_.clear();

    if (ctx_ && ctx_->renderer)
    {
        if (bounds_mesh_ != invalid_mesh)
            ctx_->renderer->destroy_mesh(bounds_mesh_);
        if (axis_mesh_ != invalid_mesh)
            ctx_->renderer->destroy_mesh(axis_mesh_);
        if (target_mesh_ != invalid_mesh)
            ctx_->renderer->destroy_mesh(target_mesh_);
    }

    ctx_ = nullptr;
}

void Editor::create_default_scene()
{
    scene_.new_scene("Demo Scene");

    // Helicopters - fast and agile
    {
        auto& u      = scene_.add_unit("Cobra", unit_type::helicopter);
        u.model_path = "assets/models/helics/cobra/cobra.obj";
        u.position   = { -15.0f, 5.0f, 0.0f };
        u.rotation   = { 0.0f, 0.0f, 0.0f };
        u.scale      = glm::vec3(0.04f);
        u.movement.flight_height   = 5.0f;
        u.movement.move_speed      = 25.0f;  // Fast
        u.movement.rotation_speed  = 120.0f; // Quick turns
        u.movement.acceleration    = 15.0f;  // Snappy acceleration
        u.movement.hover_amplitude = 0.15f;
        u.movement.hover_frequency = 0.5f;
        u.movement.mass            = 2.0f;
    }
    {
        auto& u      = scene_.add_unit("MI-24", unit_type::helicopter);
        u.model_path = "assets/models/helics/mi_24/mi_24.obj";
        u.position   = { 0.0f, 6.0f, 5.0f };
        u.rotation   = { 0.0f, 45.0f, 0.0f };
        u.scale      = glm::vec3(0.04f);
        u.movement.flight_height   = 6.0f;
        u.movement.move_speed      = 20.0f; // Heavy but fast
        u.movement.rotation_speed  = 90.0f;
        u.movement.acceleration    = 10.0f;
        u.movement.hover_amplitude = 0.12f;
        u.movement.hover_frequency = 0.4f;
        u.movement.mass            = 3.0f;
    }
    {
        auto& u      = scene_.add_unit("Kamov", unit_type::helicopter);
        u.model_path = "assets/models/helics/kamov/kamov.obj";
        u.position   = { 15.0f, 4.0f, -5.0f };
        u.rotation   = { 0.0f, -90.0f, 0.0f };
        u.scale      = glm::vec3(0.04f);
        u.movement.flight_height   = 4.0f;
        u.movement.move_speed      = 30.0f;  // Very agile
        u.movement.rotation_speed  = 150.0f; // Super fast turns
        u.movement.acceleration    = 20.0f;
        u.movement.hover_amplitude = 0.1f;
        u.movement.hover_frequency = 0.6f;
        u.movement.mass            = 1.5f;
    }

    // Tanks
    {
        auto& u      = scene_.add_unit("t72", unit_type::ground_vehicle);
        u.model_path = "assets/models/tanks/t72/t72_base.obj";
        u.position   = { 20.0f, 0.0f, 12.0f };
        u.rotation   = { 0.0f, -120.0f, 0.0f };
        u.scale      = glm::vec3(0.04f);
        u.movement.move_speed     = 5.0f;
        u.movement.rotation_speed = 30.0f;
        u.movement.acceleration   = 1.0f;
        u.movement.mass           = 10.0f;
    }
    {
        auto& u      = scene_.add_unit("Tank 2", unit_type::ground_vehicle);
        u.model_path = "assets/models/tanks/tiger/tiger_base.obj";
        u.position   = { -20.0f, 0.0f, 15.0f };
        u.rotation   = { 0.0f, 60.0f, 0.0f };
        u.scale      = glm::vec3(0.04f);
        u.movement.move_speed     = 4.0f;
        u.movement.rotation_speed = 25.0f;
        u.movement.mass           = 12.0f;
    }

    // Load models
    for (auto& u : scene_.units())
    {
        if (u.model == invalid_model && !u.model_path.empty())
            u.model = ctx_->renderer->load_model(u.model_path);
    }
}

void Editor::update(float dt)
{
    scene_.update(dt);
}

void Editor::set_move_target(const glm::vec3& target)
{
    move_target_     = target;
    move_target_set_ = true;
    scene_.command_move_to(target);
}

void Editor::handle_click(const glm::vec3& world_pos, bool is_right_click)
{
    if (is_right_click)
    {
        // Right-click: move command for selected unit
        auto* sel = scene_.get_selected();
        if (sel && movement::can_move(*sel))
        {
            set_move_target(world_pos);
        }
    }
    else
    {
        // Left-click: select unit
        try_select_at(world_pos);
    }
}

void Editor::try_select_at(const glm::vec3& world_pos)
{
    // Find closest unit to click position
    constexpr float MAX_SELECT_DIST = 8.0f;
    float           best_dist       = MAX_SELECT_DIST;
    int             best_idx        = -1;

    for (size_t i = 0; i < scene_.units().size(); ++i)
    {
        const auto& unit = scene_.units()[i];

        // For helicopters, use their actual XZ position (they're in the air)
        // The click pos is on ground, so we project helicopter to ground for
        // comparison
        glm::vec3 unit_ground_pos = unit.position;
        unit_ground_pos.y         = 0.0f;

        glm::vec3 click_ground_pos = world_pos;
        click_ground_pos.y         = 0.0f;

        float dist = glm::length(unit_ground_pos - click_ground_pos);

        // Account for unit size (scale) - larger selection radius for
        // helicopters
        float base_radius =
            (unit.movement.type == unit_type::helicopter) ? 5.0f : 3.0f;
        float select_radius = base_radius * (unit.scale.x / 0.04f);

        if (dist < select_radius && dist < best_dist)
        {
            best_dist = dist;
            best_idx  = static_cast<int>(i);
        }
    }

    if (best_idx >= 0)
        scene_.select_unit(static_cast<size_t>(best_idx));
    else
        scene_.deselect_all();
}

void Editor::draw_gizmos()
{
    if (!ctx_ || !ctx_->renderer)
        return;

    auto* selected = scene_.get_selected();
    if (selected && config_.show_bounds)
    {
        if (selected->model != invalid_model)
        {
            auto      b = ctx_->renderer->get_bounds(selected->model);
            transform xform{ selected->position,
                             selected->rotation,
                             selected->scale };
            draw_bounds_gizmo(b, xform, config_.selected_color);
        }

        if (config_.show_axes)
            draw_axis_gizmo(selected->position, config_.gizmo_scale * 3.0f);

        // Draw move target
        if (config_.show_move_target && selected->movement.has_target)
            draw_target_marker(selected->movement.target_position);
    }
}

void Editor::draw_ui()
{
    if (!visible_ || !ctx_)
        return;

    ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx_->imgui_ctx));
    draw_main_window();
    draw_file_browser_modal();
}

void Editor::draw_main_window()
{
    ImGui::SetNextWindowSize(ImVec2(380, 650), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 390, 10),
                            ImGuiCond_FirstUseEver);

    std::string title = "Scene Editor";
    if (scene_.is_dirty())
        title += " *";
    if (scene_.has_file())
        title += " - " + scene_.current_path().filename().string();
    title += "###SceneEditor";

    if (ImGui::Begin(title.c_str(), &visible_, ImGuiWindowFlags_MenuBar))
    {
        // Menu bar
        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("New Scene", "Ctrl+N"))
                {
                    scene_.new_scene();
                    create_default_scene();
                }
                if (ImGui::MenuItem("Open...", "Ctrl+O"))
                {
                    open_file_browser("Open Scene",
                                      "assets/scenes",
                                      { ".yaml", ".yml" },
                                      [this](const std::string& path)
                                      { scene_.load(path); });
                }
                if (ImGui::MenuItem("Save", "Ctrl+S", false, scene_.has_file()))
                    scene_.save();
                if (ImGui::MenuItem("Save As..."))
                {
                    open_file_browser("Save Scene",
                                      "assets/scenes",
                                      { ".yaml" },
                                      [this](const std::string& path)
                                      { scene_.save(path); });
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Create Default Scene"))
                    create_default_scene();
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("View"))
            {
                ImGui::Checkbox("Show Bounds", &config_.show_bounds);
                ImGui::Checkbox("Show Grid", &config_.show_grid);
                ImGui::Checkbox("Show Axes", &config_.show_axes);
                ImGui::Checkbox("Show Move Target", &config_.show_move_target);
                ImGui::SliderFloat(
                    "Gizmo Scale", &config_.gizmo_scale, 0.5f, 3.0f);
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        if (ImGui::BeginTabBar("EditorTabs"))
        {
            if (ImGui::BeginTabItem("Scene"))
            {
                draw_scene_panel();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Units"))
            {
                draw_units_panel();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Inspector"))
            {
                draw_inspector_panel();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Audio"))
            {
                draw_audio_panel();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}

void Editor::draw_scene_panel()
{
    ImGui::TextColored(
        { 0.6f, 0.9f, 1.0f, 1.0f }, "Scene: %s", scene_.config().name.c_str());

    static char name_buf[64];
    strncpy(name_buf, scene_.config().name.c_str(), sizeof(name_buf) - 1);
    if (ImGui::InputText("Name", name_buf, sizeof(name_buf)))
        scene_.config().name = name_buf;

    ImGui::Separator();
    ImGui::Text("Saved Scenes:");

    ImGui::BeginChild("##scenelist", ImVec2(0, 100), ImGuiChildFlags_Borders);
    for (size_t i = 0; i < scene_list_.size(); ++i)
    {
        bool selected = (static_cast<int>(i) == selected_scene_);
        if (ImGui::Selectable(scene_list_[i].c_str(), selected))
            selected_scene_ = static_cast<int>(i);

        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
        {
            std::string path = "assets/scenes/" + scene_list_[i] + ".yaml";
            scene_.load(path);
        }
    }
    ImGui::EndChild();

    if (selected_scene_ >= 0 &&
        selected_scene_ < static_cast<int>(scene_list_.size()))
    {
        if (ImGui::Button("Load", ImVec2(80, 0)))
        {
            std::string path =
                "assets/scenes/" +
                scene_list_[static_cast<size_t>(selected_scene_)] + ".yaml";
            scene_.load(path);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Save", ImVec2(80, 0)))
    {
        if (scene_.has_file())
            scene_.save();
        else
        {
            std::string path =
                "assets/scenes/" + scene_.config().name + ".yaml";
            scene_.save(path);
            scene_list_ = Scene::list_scenes("assets/scenes");
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Refresh", ImVec2(80, 0)))
        scene_list_ = Scene::list_scenes("assets/scenes");

    ImGui::Separator();
    ImGui::TextColored(
        { 0.7f, 0.7f, 0.7f, 1.0f }, "Units: %zu", scene_.units().size());
}

void Editor::draw_units_panel()
{
    ImGui::TextColored(
        { 0.6f, 0.9f, 1.0f, 1.0f }, "Units (%zu)", scene_.units().size());

    // Unit list
    ImGui::BeginChild("##unitlist", ImVec2(0, -140), ImGuiChildFlags_Borders);
    auto& units = scene_.units();
    for (size_t i = 0; i < units.size(); ++i)
    {
        auto& unit = units[i];
        ImGui::PushID(static_cast<int>(i));

        bool selected = (scene_.get_selected_index() == static_cast<int>(i));

        // Icon based on type
        const char* icon = "";
        switch (unit.movement.type)
        {
            case unit_type::helicopter:
                icon = "[H]";
                break;
            case unit_type::ground_vehicle:
                icon = "[V]";
                break;
            case unit_type::aircraft:
                icon = "[A]";
                break;
            default:
                icon = "[S]";
                break;
        }

        // State indicator
        const char* state = "";
        if (unit.movement.type != unit_type::static_object)
        {
            switch (unit.movement.state)
            {
                case move_state::moving:
                    state = " >>";
                    break;
                case move_state::rotating:
                    state = " <>";
                    break;
                case move_state::hovering:
                    state = " ~~";
                    break;
                default:
                    break;
            }
        }

        std::string label = std::string(icon) + " " + unit.name + state;

        if (ImGui::Selectable(label.c_str(), selected))
            scene_.select_unit(i);

        if (ImGui::BeginPopupContextItem())
        {
            if (ImGui::MenuItem("Duplicate"))
                scene_.duplicate_unit(i);
            if (ImGui::MenuItem("Delete"))
                scene_.remove_unit(i);
            ImGui::Separator();
            if (ImGui::MenuItem("Stop"))
                movement::stop(unit);
            ImGui::EndPopup();
        }

        ImGui::PopID();
    }
    ImGui::EndChild();

    // Add unit buttons
    ImGui::Text("Add Unit:");
    if (ImGui::Button("+ Helicopter", ImVec2(-1, 0)))
    {
        auto& u = scene_.add_unit("New Helicopter", unit_type::helicopter);
        u.scale = glm::vec3(0.04f);
        open_file_browser("Select Model",
                          "assets/models/helics",
                          { ".obj" },
                          [this, &u](const std::string& path)
                          {
                              u.model_path = path;
                              u.model      = ctx_->renderer->load_model(path);
                          });
    }
    if (ImGui::Button("+ Ground Vehicle", ImVec2(-1, 0)))
    {
        auto& u = scene_.add_unit("New Vehicle", unit_type::ground_vehicle);
        u.scale = glm::vec3(0.04f);
        open_file_browser("Select Model",
                          "assets/models",
                          { ".obj" },
                          [this, &u](const std::string& path)
                          {
                              u.model_path = path;
                              u.model      = ctx_->renderer->load_model(path);
                          });
    }
    if (ImGui::Button("+ Static Object", ImVec2(-1, 0)))
    {
        auto& u = scene_.add_unit("New Object", unit_type::static_object);
        u.scale = glm::vec3(0.04f);
        open_file_browser("Select Model",
                          "assets/models",
                          { ".obj" },
                          [this, &u](const std::string& path)
                          {
                              u.model_path = path;
                              u.model      = ctx_->renderer->load_model(path);
                          });
    }

    // Movement controls
    auto* sel = scene_.get_selected();
    if (sel && movement::can_move(*sel))
    {
        ImGui::Separator();
        ImGui::TextColored({ 1.0f, 0.6f, 0.4f, 1.0f }, "Movement Command:");

        static float target_x = 0.0f, target_z = 0.0f;
        ImGui::SetNextItemWidth(80);
        ImGui::DragFloat("##tx", &target_x, 0.5f, -100.0f, 100.0f, "X:%.1f");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80);
        ImGui::DragFloat("##tz", &target_z, 0.5f, -100.0f, 100.0f, "Z:%.1f");
        ImGui::SameLine();
        if (ImGui::Button("Move To"))
        {
            float y = (sel->movement.type == unit_type::helicopter)
                          ? sel->movement.flight_height
                          : 0.0f;
            set_move_target(glm::vec3(target_x, y, target_z));
        }

        if (ImGui::Button("Stop", ImVec2(-1, 0)))
            movement::stop(*sel);
    }
}

void Editor::draw_inspector_panel()
{
    auto* unit = scene_.get_selected();
    if (!unit)
    {
        ImGui::TextColored({ 0.5f, 0.5f, 0.5f, 1.0f }, "No unit selected");
        return;
    }

    ImGui::TextColored(
        { 1.0f, 0.9f, 0.4f, 1.0f }, "Inspector: %s", unit->name.c_str());
    ImGui::Separator();

    // Name
    static char name_buf[64];
    strncpy(name_buf, unit->name.c_str(), sizeof(name_buf) - 1);
    if (ImGui::InputText("Name", name_buf, sizeof(name_buf)))
        unit->name = name_buf;

    // Type
    int type_idx = static_cast<int>(unit->movement.type);
    if (ImGui::Combo("Type", &type_idx, unit_type_names, 4))
        unit->movement.type = static_cast<unit_type>(type_idx);

    // Model
    ImGui::Text("Model: %s",
                unit->model_path.empty()
                    ? "(none)"
                    : std::filesystem::path(unit->model_path)
                          .filename()
                          .string()
                          .c_str());
    ImGui::SameLine();
    if (ImGui::SmallButton("...##mdl"))
    {
        open_file_browser("Select Model",
                          "assets/models",
                          { ".obj" },
                          [this, unit](const std::string& path)
                          {
                              unit->model_path = path;
                              if (unit->model != invalid_model)
                                  ctx_->renderer->unload_model(unit->model);
                              unit->model = ctx_->renderer->load_model(path);
                          });
    }

    ImGui::Separator();
    ImGui::TextColored({ 0.4f, 0.8f, 1.0f, 1.0f }, "Transform");

    ImGui::DragFloat3("Position", &unit->position.x, 0.1f);
    ImGui::DragFloat3("Rotation", &unit->rotation.x, 1.0f, -360.0f, 360.0f);

    float scale = unit->scale.x;
    if (ImGui::DragFloat("Scale", &scale, 0.001f, 0.001f, 10.0f))
        unit->scale = glm::vec3(scale);

    // Movement settings
    if (unit->movement.type != unit_type::static_object)
    {
        ImGui::Separator();
        ImGui::TextColored({ 1.0f, 0.6f, 0.4f, 1.0f }, "Movement");

        ImGui::Text("State: %s",
                    move_state_names[static_cast<int>(unit->movement.state)]);
        ImGui::Text("Speed: %.1f / %.1f",
                    unit->movement.current_speed,
                    unit->movement.move_speed);

        ImGui::DragFloat(
            "Max Speed", &unit->movement.move_speed, 0.1f, 1.0f, 50.0f);
        ImGui::DragFloat(
            "Turn Speed", &unit->movement.rotation_speed, 1.0f, 10.0f, 360.0f);
        ImGui::DragFloat(
            "Acceleration", &unit->movement.acceleration, 0.1f, 1.0f, 20.0f);

        if (unit->movement.type == unit_type::helicopter)
        {
            ImGui::DragFloat("Flight Height",
                             &unit->movement.flight_height,
                             0.1f,
                             1.0f,
                             50.0f);
            ImGui::DragFloat("Hover Amplitude",
                             &unit->movement.hover_amplitude,
                             0.01f,
                             0.0f,
                             2.0f);
        }

        if (unit->movement.has_target)
        {
            ImGui::Separator();
            ImGui::Text("Target: %.1f, %.1f, %.1f",
                        unit->movement.target_position.x,
                        unit->movement.target_position.y,
                        unit->movement.target_position.z);
            float dist =
                glm::length(unit->movement.target_position - unit->position);
            ImGui::Text("Distance: %.1f", dist);
        }
    }
}

void Editor::draw_audio_panel()
{
    if (!ctx_ || !ctx_->audio)
    {
        ImGui::Text("Audio not available");
        return;
    }

    auto* audio = ctx_->audio;

    // === MUSIC SECTION ===
    ImGui::TextColored(
        { 0.4f, 1.0f, 0.6f, 1.0f }, "Music (%zu tracks)", music_files_.size());

    float music_vol = audio->get_music_volume();
    if (ImGui::SliderFloat("Music Vol", &music_vol, 0.0f, 1.0f))
        audio->set_music_volume(music_vol);

    // Transport controls
    bool playing = audio->is_music_playing();
    bool paused  = audio->is_music_paused();

    if (playing && !paused)
    {
        if (ImGui::Button("||##m", ImVec2(30, 0)))
            audio->pause_music();
    }
    else
    {
        if (ImGui::Button(">##m", ImVec2(30, 0)))
        {
            if (paused)
                audio->resume_music();
            else if (current_music_ >= 0 &&
                     current_music_ < static_cast<int>(loaded_music_.size()))
            {
                if (loaded_music_[static_cast<size_t>(current_music_)] !=
                    invalid_music)
                    audio->play_music(
                        loaded_music_[static_cast<size_t>(current_music_)]);
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("[]##m", ImVec2(30, 0)))
        audio->stop_music();
    ImGui::SameLine();
    if (current_music_ >= 0 &&
        current_music_ < static_cast<int>(music_files_.size()))
        ImGui::Text("Now: %s",
                    music_files_[static_cast<size_t>(current_music_)].c_str());
    else
        ImGui::Text("Now: (none)");

    ImGui::BeginChild("##musiclist", ImVec2(0, 100), ImGuiChildFlags_Borders);
    for (size_t i = 0; i < music_files_.size(); ++i)
    {
        ImGui::PushID(static_cast<int>(i));

        bool is_current = (static_cast<int>(i) == current_music_);
        if (is_current)
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  ImVec4(0.3f, 1.0f, 0.3f, 1.0f));

        if (ImGui::Selectable(music_files_[i].c_str(),
                              static_cast<int>(i) == selected_music_))
            selected_music_ = static_cast<int>(i);

        // Double-click to play
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
        {
            while (loaded_music_.size() <= i)
                loaded_music_.push_back(invalid_music);

            if (loaded_music_[i] == invalid_music)
                loaded_music_[i] =
                    audio->load_music("assets/music/" + music_files_[i]);

            if (loaded_music_[i] != invalid_music)
            {
                audio->play_music(loaded_music_[i]);
                current_music_ = static_cast<int>(i);
            }
        }

        if (is_current)
            ImGui::PopStyleColor();
        ImGui::PopID();
    }
    ImGui::EndChild();

    ImGui::Separator();

    // === SOUNDS SECTION ===
    ImGui::TextColored(
        { 1.0f, 0.8f, 0.4f, 1.0f }, "Sound Effects (%zu)", sound_files_.size());

    float sfx_vol = audio->get_sound_volume();
    if (ImGui::SliderFloat("SFX Vol", &sfx_vol, 0.0f, 1.0f))
        audio->set_sound_volume(sfx_vol);

    // Filter
    static char filter[32] = "";
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint(
        "##filter", "Filter sounds...", filter, sizeof(filter));

    ImGui::BeginChild("##soundlist", ImVec2(0, 150), ImGuiChildFlags_Borders);
    for (size_t i = 0; i < sound_files_.size(); ++i)
    {
        // Apply filter
        if (filter[0] != '\0')
        {
            std::string lower = sound_files_[i];
            std::transform(
                lower.begin(), lower.end(), lower.begin(), ::tolower);
            std::string filter_lower = filter;
            std::transform(filter_lower.begin(),
                           filter_lower.end(),
                           filter_lower.begin(),
                           ::tolower);
            if (lower.find(filter_lower) == std::string::npos)
                continue;
        }

        ImGui::PushID(static_cast<int>(i + 1000));

        if (ImGui::Selectable(sound_files_[i].c_str(),
                              static_cast<int>(i) == selected_sound_))
            selected_sound_ = static_cast<int>(i);

        // Double-click to play
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
        {
            while (loaded_sounds_.size() <= i)
                loaded_sounds_.push_back(invalid_sound);

            if (loaded_sounds_[i] == invalid_sound)
                loaded_sounds_[i] =
                    audio->load_sound("assets/sounds/" + sound_files_[i]);

            if (loaded_sounds_[i] != invalid_sound)
                audio->play_sound(loaded_sounds_[i]);
        }

        ImGui::PopID();
    }
    ImGui::EndChild();

    if (selected_sound_ >= 0 &&
        selected_sound_ < static_cast<int>(sound_files_.size()))
    {
        if (ImGui::Button("Play Sound", ImVec2(-1, 0)))
        {
            size_t idx = static_cast<size_t>(selected_sound_);
            while (loaded_sounds_.size() <= idx)
                loaded_sounds_.push_back(invalid_sound);

            if (loaded_sounds_[idx] == invalid_sound)
                loaded_sounds_[idx] =
                    audio->load_sound("assets/sounds/" + sound_files_[idx]);

            if (loaded_sounds_[idx] != invalid_sound)
                audio->play_sound(loaded_sounds_[idx]);
        }
    }
}

void Editor::draw_file_browser_modal()
{
    if (!file_browser_.open)
        return;

    ImGui::OpenPopup(file_browser_.title.c_str());

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(450, 350), ImGuiCond_FirstUseEver);

    if (ImGui::BeginPopupModal(file_browser_.title.c_str(),
                               &file_browser_.open))
    {
        if (file_browser_.needs_refresh)
            refresh_file_browser();

        ImGui::Text("Path: %s", file_browser_.current_path.c_str());

        if (ImGui::Button("^ Up"))
        {
            std::filesystem::path p(file_browser_.current_path);
            if (p.has_parent_path() && p.parent_path() != p)
            {
                file_browser_.current_path  = p.parent_path().string();
                file_browser_.needs_refresh = true;
            }
        }

        ImGui::Separator();

        ImGui::BeginChild("##browser", ImVec2(0, -50), ImGuiChildFlags_Borders);

        for (const auto& dir : file_browser_.directories)
        {
            std::string label = "[D] " + dir;
            if (ImGui::Selectable(label.c_str(),
                                  false,
                                  ImGuiSelectableFlags_AllowDoubleClick))
            {
                if (ImGui::IsMouseDoubleClicked(0))
                {
                    file_browser_.current_path += "/" + dir;
                    file_browser_.needs_refresh = true;
                }
            }
        }

        for (const auto& file : file_browser_.files)
        {
            bool selected = (file == file_browser_.selected_file);
            if (ImGui::Selectable(file.c_str(), selected))
                file_browser_.selected_file = file;

            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
            {
                std::string full = file_browser_.current_path + "/" + file;
                if (file_browser_.on_select)
                    file_browser_.on_select(full);
                file_browser_.open = false;
                ImGui::CloseCurrentPopup();
            }
        }

        ImGui::EndChild();

        if (ImGui::Button("Select", ImVec2(100, 0)) &&
            !file_browser_.selected_file.empty())
        {
            std::string full =
                file_browser_.current_path + "/" + file_browser_.selected_file;
            if (file_browser_.on_select)
                file_browser_.on_select(full);
            file_browser_.open = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0)))
        {
            file_browser_.open = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void Editor::refresh_file_browser()
{
    file_browser_.directories.clear();
    file_browser_.files.clear();
    file_browser_.needs_refresh = false;

    try
    {
        for (const auto& entry :
             std::filesystem::directory_iterator(file_browser_.current_path))
        {
            std::string name = entry.path().filename().string();
            if (name.empty() || name[0] == '.')
                continue;

            if (entry.is_directory())
            {
                file_browser_.directories.push_back(name);
            }
            else if (entry.is_regular_file())
            {
                auto ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

                bool match = file_browser_.extensions.empty();
                for (const auto& e : file_browser_.extensions)
                    if (ext == e)
                    {
                        match = true;
                        break;
                    }

                if (match)
                    file_browser_.files.push_back(name);
            }
        }
        std::sort(file_browser_.directories.begin(),
                  file_browser_.directories.end());
        std::sort(file_browser_.files.begin(), file_browser_.files.end());
    }
    catch (const std::exception& e)
    {
        spdlog::error("File browser: {}", e.what());
    }
}

void Editor::open_file_browser(const std::string&              title,
                               const std::string&              start_path,
                               const std::vector<std::string>& extensions,
                               std::function<void(const std::string&)> callback)
{
    file_browser_.open         = true;
    file_browser_.title        = title;
    file_browser_.current_path = start_path;
    file_browser_.selected_file.clear();
    file_browser_.extensions    = extensions;
    file_browser_.on_select     = std::move(callback);
    file_browser_.needs_refresh = true;
}

void Editor::draw_bounds_gizmo(const bounds&    b,
                               const transform& xform,
                               const glm::vec3& color)
{
    auto min    = b.min * xform.scale + xform.position;
    auto max    = b.max * xform.scale + xform.position;
    auto center = (min + max) * 0.5f;
    auto size   = (max - min);

    if (bounds_mesh_ != invalid_mesh)
    {
        ctx_->renderer->destroy_mesh(bounds_mesh_);
        bounds_mesh_ = invalid_mesh;
    }

    bounds_mesh_ = ctx_->renderer->create_wireframe_cube(center, size.x, color);
    if (bounds_mesh_ != invalid_mesh)
        ctx_->renderer->draw(bounds_mesh_);
}

void Editor::draw_axis_gizmo(const glm::vec3& pos, float size)
{
    std::vector<vertex>   verts;
    std::vector<uint16_t> indices;

    verts.push_back({ pos, { 1.0f, 0.0f, 0.0f } });
    verts.push_back(
        { pos + glm::vec3(size, 0.0f, 0.0f), { 1.0f, 0.0f, 0.0f } });
    verts.push_back({ pos, { 0.0f, 1.0f, 0.0f } });
    verts.push_back(
        { pos + glm::vec3(0.0f, size, 0.0f), { 0.0f, 1.0f, 0.0f } });
    verts.push_back({ pos, { 0.0f, 0.0f, 1.0f } });
    verts.push_back(
        { pos + glm::vec3(0.0f, 0.0f, size), { 0.0f, 0.0f, 1.0f } });

    indices = { 0, 1, 2, 3, 4, 5 };

    if (axis_mesh_ != invalid_mesh)
    {
        ctx_->renderer->destroy_mesh(axis_mesh_);
        axis_mesh_ = invalid_mesh;
    }

    axis_mesh_ =
        ctx_->renderer->create_mesh(verts, indices, primitive_type::lines);
    if (axis_mesh_ != invalid_mesh)
        ctx_->renderer->draw(axis_mesh_);
}

void Editor::draw_target_marker(const glm::vec3& pos)
{
    // Draw X marker at target position
    std::vector<vertex>   verts;
    std::vector<uint16_t> indices;

    float     s = 1.0f;
    glm::vec3 c = config_.target_color;

    // X shape
    verts.push_back({ pos + glm::vec3(-s, 0.1f, -s), c });
    verts.push_back({ pos + glm::vec3(s, 0.1f, s), c });
    verts.push_back({ pos + glm::vec3(s, 0.1f, -s), c });
    verts.push_back({ pos + glm::vec3(-s, 0.1f, s), c });

    // Vertical line
    verts.push_back({ pos + glm::vec3(0.0f, 0.0f, 0.0f), c });
    verts.push_back({ pos + glm::vec3(0.0f, 3.0f, 0.0f), c });

    indices = { 0, 1, 2, 3, 4, 5 };

    if (target_mesh_ != invalid_mesh)
    {
        ctx_->renderer->destroy_mesh(target_mesh_);
        target_mesh_ = invalid_mesh;
    }

    target_mesh_ =
        ctx_->renderer->create_mesh(verts, indices, primitive_type::lines);
    if (target_mesh_ != invalid_mesh)
        ctx_->renderer->draw(target_mesh_);
}

} // namespace as3