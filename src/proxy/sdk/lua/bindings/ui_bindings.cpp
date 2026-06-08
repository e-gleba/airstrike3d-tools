#include "sdk/lua/bindings/bindings_fwd.hpp"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <ranges>
#include <sol/sol.hpp>
#include <string>
#include <tuple>

namespace sdk::lua::bindings
{

void register_ui(sol::state& sol_state)
{
    auto ui{ sol_state.create_named_table("ui") };

    auto bind_text_fn = [&](const char* name, auto imgui_fn)
    {
        ui.set_function(name,
                        [=](const std::string& t) noexcept
                        { imgui_fn("%s", t.c_str()); });
    };

    auto bind_void_fn = [&](const char* name, auto imgui_fn)
    { ui.set_function(name, [=]() noexcept { imgui_fn(); }); };

    auto bind_begin_fn = [&](const char* name, auto imgui_fn)
    {
        ui.set_function(name,
                        [=](const std::string& l) noexcept -> bool
                        { return imgui_fn(l.c_str()); });
    };

    bind_begin_fn("begin_window",
                  [](const char* s) noexcept { return ImGui::Begin(s); });
    bind_void_fn("end_window", []() noexcept { ImGui::End(); });

    bind_text_fn("text",
                 [](const char* fmt, const char* s) noexcept
                 { ImGui::Text(fmt, s); });
    bind_text_fn("text_wrapped",
                 [](const char* fmt, const char* s) noexcept
                 { ImGui::TextWrapped(fmt, s); });
    bind_text_fn("text_disabled",
                 [](const char* fmt, const char* s) noexcept
                 { ImGui::TextDisabled(fmt, s); });

    ui.set_function(
        "text_colored",
        [](float r, float g, float b, float a, const std::string& t) noexcept
        { ImGui::TextColored({ r, g, b, a }, "%s", t.c_str()); });

    ui.set_function("button",
                    [](const std::string& l) noexcept -> bool
                    { return ImGui::Button(l.c_str(), { -1, 0 }); });

    ui.set_function("button_sized",
                    [](const std::string& l, float w, float h) noexcept -> bool
                    { return ImGui::Button(l.c_str(), { w, h }); });

    ui.set_function(
        "checkbox",
        [](const std::string& l, bool v) noexcept -> std::tuple<bool, bool>
        {
            auto c{ ImGui::Checkbox(l.c_str(), &v) };
            return { v, c };
        });

    ui.set_function("drag_float",
                    [](const std::string& l,
                       float              v,
                       float              spd,
                       float              mn,
                       float mx) noexcept -> std::tuple<float, bool>
                    {
                        auto c{ ImGui::DragFloat(l.c_str(), &v, spd, mn, mx) };
                        return { v, c };
                    });

    ui.set_function("slider_float",
                    [](const std::string& l,
                       float              v,
                       float              mn,
                       float mx) noexcept -> std::tuple<float, bool>
                    {
                        auto c{ ImGui::SliderFloat(l.c_str(), &v, mn, mx) };
                        return { v, c };
                    });

    ui.set_function("slider_int",
                    [](const std::string& l, int v, int mn, int mx) noexcept
                        -> std::tuple<int, bool>
                    {
                        auto c{ ImGui::SliderInt(l.c_str(), &v, mn, mx) };
                        return { v, c };
                    });

    ui.set_function("input_text",
                    [](const std::string& label, std::string text) noexcept
                        -> std::tuple<std::string, bool>
                    {
                        auto c{ ImGui::InputText(label.c_str(), &text) };
                        return { std::move(text), c };
                    });

    ui.set_function("color_edit3",
                    [](const std::string& l, float r, float g, float b) noexcept
                        -> std::tuple<float, float, float, bool>
                    {
                        std::array col{ r, g, b };
                        auto c{ ImGui::ColorEdit3(l.c_str(), col.data()) };
                        return { col[0], col[1], col[2], c };
                    });

    ui.set_function(
        "combo",
        [](const std::string& label,
           int                current,
           sol::table         items) -> std::tuple<int, bool>
        {
            auto strs{ items | std::views::values |
                       std::views::transform(
                           [](const auto& v)
                           { return v.template as<std::string>(); }) |
                       std::ranges::to<std::vector<std::string>>() };

            auto ptrs{ strs | std::views::transform(&std::string::c_str) |
                       std::ranges::to<std::vector<const char*>>() };

            auto c{ ImGui::Combo(label.c_str(),
                                 &current,
                                 ptrs.data(),
                                 static_cast<int>(ptrs.size())) };
            return { current, c };
        });

    ui.set_function("progress_bar",
                    [](float fraction, const std::string& overlay) noexcept
                    {
                        ImGui::ProgressBar(fraction,
                                           { -1, 0 },
                                           overlay.empty() ? nullptr
                                                           : overlay.c_str());
                    });

    ui.set_function("tooltip",
                    [](const std::string& t) noexcept
                    {
                        if (ImGui::IsItemHovered())
                        {
                            ImGui::SetTooltip("%s", t.c_str());
                        }
                    });

    bind_begin_fn("tree_node",
                  [](const char* s) noexcept { return ImGui::TreeNode(s); });
    bind_void_fn("tree_pop", []() noexcept { ImGui::TreePop(); });

    bind_begin_fn("tab_bar_begin",
                  [](const char* s) noexcept { return ImGui::BeginTabBar(s); });
    bind_void_fn("tab_bar_end", []() noexcept { ImGui::EndTabBar(); });
    bind_begin_fn("tab_item_begin",
                  [](const char* s) noexcept
                  { return ImGui::BeginTabItem(s); });
    bind_void_fn("tab_item_end", []() noexcept { ImGui::EndTabItem(); });

    bind_void_fn("separator", []() noexcept { ImGui::Separator(); });
    bind_void_fn("same_line", []() noexcept { ImGui::SameLine(); });
    bind_void_fn("spacing", []() noexcept { ImGui::Spacing(); });

    ui.set_function("collapsing_header",
                    [](const std::string& l, bool open) noexcept -> bool
                    {
                        return ImGui::CollapsingHeader(
                            l.c_str(),
                            open ? ImGuiTreeNodeFlags_DefaultOpen : 0);
                    });

    ui.set_function(
        "set_next_window_pos",
        [](float x, float y) noexcept
        { ImGui::SetNextWindowPos({ x, y }, ImGuiCond_FirstUseEver); });

    ui.set_function(
        "set_next_window_size",
        [](float w, float h) noexcept
        { ImGui::SetNextWindowSize({ w, h }, ImGuiCond_FirstUseEver); });

    ui.set_function("set_cursor_pos_x",
                    [](float x) noexcept { ImGui::SetCursorPosX(x); });
    ui.set_function("get_window_width",
                    []() noexcept -> float { return ImGui::GetWindowWidth(); });

    ui.set_function("push_style_color",
                    [](int idx, float r, float g, float b, float a) noexcept
                    { ImGui::PushStyleColor(idx, ImVec4(r, g, b, a)); });

    bind_void_fn("pop_style_color", []() noexcept { ImGui::PopStyleColor(); });

    ui.set_function("push_style_var_float",
                    [](int idx, float v) noexcept
                    { ImGui::PushStyleVar(idx, v); });

    ui.set_function("push_style_var_vec2",
                    [](int idx, float x, float y) noexcept
                    { ImGui::PushStyleVar(idx, ImVec2(x, y)); });

    bind_void_fn("pop_style_var", []() noexcept { ImGui::PopStyleVar(); });

    ui.set_function("columns",
                    [](int count, const std::string& id, bool border) noexcept
                    { ImGui::Columns(count, id.c_str(), border); });

    bind_void_fn("next_column", []() noexcept { ImGui::NextColumn(); });

    ui.set_function("set_column_width",
                    [](int idx, float w) noexcept
                    { ImGui::SetColumnWidth(idx, w); });

    ui.set_function("get_delta_time",
                    []() noexcept -> float
                    { return ImGui::GetIO().DeltaTime; });
    ui.set_function("get_framerate",
                    []() noexcept -> float
                    { return ImGui::GetIO().Framerate; });
    ui.set_function("want_capture_keyboard",
                    []() noexcept -> bool
                    { return ImGui::GetIO().WantCaptureKeyboard; });
    ui.set_function("want_capture_mouse",
                    []() noexcept -> bool
                    { return ImGui::GetIO().WantCaptureMouse; });
}

} // namespace sdk::lua::bindings
