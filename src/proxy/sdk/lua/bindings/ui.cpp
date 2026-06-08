/// @file ui.cpp
/// @brief Lua bindings for ImGui UI framework.

#include "bindings.hpp"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <ranges>
#include <string>
#include <tuple>

namespace sdk::lua::bindings
{

void register_ui(sol::state& state)
{
    auto ui{state.create_named_table("ui")};

    // Helper lambdas for common binding patterns
    auto bind_text_fn = [&](const char* name, auto imgui_fn)
    {
        ui.set_function(name,
                        [=](const std::string& text) noexcept { imgui_fn("%s", text.c_str()); });
    };

    auto bind_void_fn = [&](const char* name, auto imgui_fn)
    { ui.set_function(name, [=]() noexcept { imgui_fn(); }); };

    auto bind_begin_fn = [&](const char* name, auto imgui_fn)
    {
        ui.set_function(name,
                        [=](const std::string& label) noexcept -> bool { return imgui_fn(label.c_str()); });
    };

    // Window management
    bind_begin_fn("begin_window", [](const char* s) noexcept { return ImGui::Begin(s); });
    bind_void_fn("end_window", []() noexcept { ImGui::End(); });

    // Text display
    bind_text_fn("text", [](const char* fmt, const char* s) noexcept { ImGui::Text(fmt, s); });
    bind_text_fn("text_wrapped",
                 [](const char* fmt, const char* s) noexcept { ImGui::TextWrapped(fmt, s); });
    bind_text_fn("text_disabled",
                 [](const char* fmt, const char* s) noexcept { ImGui::TextDisabled(fmt, s); });

    ui.set_function("text_colored",
                    [](const float r, const float g, const float b, const float a,
                       const std::string& text) noexcept
                    { ImGui::TextColored({r, g, b, a}, "%s", text.c_str()); });

    // Buttons
    ui.set_function("button",
                    [](const std::string& label) noexcept -> bool
                    { return ImGui::Button(label.c_str(), {-1, 0}); });

    ui.set_function("button_sized",
                    [](const std::string& label, const float w, const float h) noexcept -> bool
                    { return ImGui::Button(label.c_str(), {w, h}); });

    // Input widgets
    ui.set_function("checkbox",
                    [](const std::string& label, bool value) noexcept -> std::tuple<bool, bool>
                    {
                        const auto changed{ImGui::Checkbox(label.c_str(), &value)};
                        return {value, changed};
                    });

    ui.set_function("drag_float",
                    [](const std::string& label, const float value,
                       const float speed, const float min_val, const float max_val) noexcept
                        -> std::tuple<float, bool>
                    {
                        float v{value};
                        const auto changed{ImGui::DragFloat(label.c_str(), &v, speed, min_val, max_val)};
                        return {v, changed};
                    });

    ui.set_function("slider_float",
                    [](const std::string& label, const float value,
                       const float min_val, const float max_val) noexcept -> std::tuple<float, bool>
                    {
                        float v{value};
                        const auto changed{ImGui::SliderFloat(label.c_str(), &v, min_val, max_val)};
                        return {v, changed};
                    });

    ui.set_function("slider_int",
                    [](const std::string& label, const int value,
                       const int min_val, const int max_val) noexcept -> std::tuple<int, bool>
                    {
                        int v{value};
                        const auto changed{ImGui::SliderInt(label.c_str(), &v, min_val, max_val)};
                        return {v, changed};
                    });

    ui.set_function("input_text",
                    [](const std::string& label, std::string text) noexcept
                        -> std::tuple<std::string, bool>
                    {
                        const auto changed{ImGui::InputText(label.c_str(), &text)};
                        return {std::move(text), changed};
                    });

    ui.set_function("color_edit3",
                    [](const std::string& label, const float r, const float g, const float b) noexcept
                        -> std::tuple<float, float, float, bool>
                    {
                        std::array col{r, g, b};
                        const auto changed{ImGui::ColorEdit3(label.c_str(), col.data())};
                        return {col[0], col[1], col[2], changed};
                    });

    ui.set_function("combo",
                    [](const std::string& label, const int current, sol::table items) -> std::tuple<int, bool>
                    {
                        auto strs{items | std::views::values |
                                  std::views::transform([](const auto& v)
                                                        { return v.template as<std::string>(); }) |
                                  std::ranges::to<std::vector<std::string>>()};

                        auto ptrs{strs | std::views::transform(&std::string::c_str) |
                                  std::ranges::to<std::vector<const char*>>()};

                        int selected{current};
                        const auto changed{ImGui::Combo(label.c_str(),
                                                        &selected,
                                                        ptrs.data(),
                                                        static_cast<int>(ptrs.size()))};
                        return {selected, changed};
                    });

    ui.set_function("progress_bar",
                    [](const float fraction, const std::string& overlay) noexcept
                    {
                        ImGui::ProgressBar(fraction,
                                           {-1, 0},
                                           overlay.empty() ? nullptr : overlay.c_str());
                    });

    ui.set_function("tooltip",
                    [](const std::string& text) noexcept
                    {
                        if (ImGui::IsItemHovered())
                        {
                            ImGui::SetTooltip("%s", text.c_str());
                        }
                    });

    // Tree and tab structure
    bind_begin_fn("tree_node", [](const char* s) noexcept { return ImGui::TreeNode(s); });
    bind_void_fn("tree_pop", []() noexcept { ImGui::TreePop(); });

    bind_begin_fn("tab_bar_begin", [](const char* s) noexcept { return ImGui::BeginTabBar(s); });
    bind_void_fn("tab_bar_end", []() noexcept { ImGui::EndTabBar(); });
    bind_begin_fn("tab_item_begin", [](const char* s) noexcept { return ImGui::BeginTabItem(s); });
    bind_void_fn("tab_item_end", []() noexcept { ImGui::EndTabItem(); });

    // Layout
    bind_void_fn("separator", []() noexcept { ImGui::Separator(); });
    bind_void_fn("same_line", []() noexcept { ImGui::SameLine(); });
    bind_void_fn("spacing", []() noexcept { ImGui::Spacing(); });

    ui.set_function("collapsing_header",
                    [](const std::string& label, const bool open) noexcept -> bool
                    {
                        return ImGui::CollapsingHeader(label.c_str(),
                                                       open ? ImGuiTreeNodeFlags_DefaultOpen : 0);
                    });

    // Window positioning and sizing
    ui.set_function("set_next_window_pos",
                    [](const float x, const float y) noexcept
                    { ImGui::SetNextWindowPos({x, y}, ImGuiCond_FirstUseEver); });

    ui.set_function("set_next_window_size",
                    [](const float w, const float h) noexcept
                    { ImGui::SetNextWindowSize({w, h}, ImGuiCond_FirstUseEver); });

    ui.set_function("set_cursor_pos_x", [](const float x) noexcept { ImGui::SetCursorPosX(x); });
    ui.set_function("get_window_width", []() noexcept -> float { return ImGui::GetWindowWidth(); });

    // Styling
    ui.set_function("push_style_color",
                    [](const int idx, const float r, const float g, const float b, const float a) noexcept
                    { ImGui::PushStyleColor(idx, ImVec4(r, g, b, a)); });

    bind_void_fn("pop_style_color", []() noexcept { ImGui::PopStyleColor(); });

    ui.set_function("push_style_var_float",
                    [](const int idx, const float v) noexcept { ImGui::PushStyleVar(idx, v); });

    ui.set_function("push_style_var_vec2",
                    [](const int idx, const float x, const float y) noexcept
                    { ImGui::PushStyleVar(idx, ImVec2(x, y)); });

    bind_void_fn("pop_style_var", []() noexcept { ImGui::PopStyleVar(); });

    // Columns
    ui.set_function("columns",
                    [](const int count, const std::string& id, const bool border) noexcept
                    { ImGui::Columns(count, id.c_str(), border); });

    bind_void_fn("next_column", []() noexcept { ImGui::NextColumn(); });

    ui.set_function("set_column_width",
                    [](const int idx, const float w) noexcept { ImGui::SetColumnWidth(idx, w); });

    // IO queries
    ui.set_function("get_delta_time", []() noexcept -> float { return ImGui::GetIO().DeltaTime; });
    ui.set_function("get_framerate", []() noexcept -> float { return ImGui::GetIO().Framerate; });
    ui.set_function("want_capture_keyboard",
                    []() noexcept -> bool { return ImGui::GetIO().WantCaptureKeyboard; });
    ui.set_function("want_capture_mouse",
                    []() noexcept -> bool { return ImGui::GetIO().WantCaptureMouse; });
}

} // namespace sdk::lua::bindings
