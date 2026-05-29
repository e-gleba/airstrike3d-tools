#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <ranges>
#include <sol/sol.hpp>
#include <string>
#include <tuple>

namespace sdk::lua
{

void register_ui_bindings(sol::state& sol_state)
{
    auto ui = sol_state.create_named_table("ui");

    // Helper: bind a simple ImGui function that takes a string label
    auto bind_text_fn = [&](const char* name, auto imgui_fn)
    {
        ui.set_function(
            name, [=](const std::string& t) { imgui_fn("%s", t.c_str()); });
    };

    // Helper: bind a simple void ImGui function (no args)
    auto bind_void_fn = [&](const char* name, auto imgui_fn)
    { ui.set_function(name, [=] { imgui_fn(); }); };

    // Helper: bind a bool-returning ImGui function taking a string
    auto bind_begin_fn = [&](const char* name, auto imgui_fn)
    {
        ui.set_function(name,
                        [=](const std::string& l) -> bool
                        { return imgui_fn(l.c_str()); });
    };

    // -- Windows --
    bind_begin_fn("begin_window",
                  [](const char* s) { return ImGui::Begin(s); });
    bind_void_fn("end_window", [] { ImGui::End(); });

    // -- Text --
    bind_text_fn("text",
                 [](const char* fmt, const char* s) { ImGui::Text(fmt, s); });
    bind_text_fn("text_wrapped",
                 [](const char* fmt, const char* s)
                 { ImGui::TextWrapped(fmt, s); });
    bind_text_fn("text_disabled",
                 [](const char* fmt, const char* s)
                 { ImGui::TextDisabled(fmt, s); });

    ui.set_function("text_colored",
                    [](float r, float g, float b, float a, const std::string& t)
                    { ImGui::TextColored({ r, g, b, a }, "%s", t.c_str()); });

    // -- Buttons --
    ui.set_function("button",
                    [](const std::string& l) -> bool
                    { return ImGui::Button(l.c_str(), { -1, 0 }); });

    ui.set_function("button_sized",
                    [](const std::string& l, float w, float h) -> bool
                    { return ImGui::Button(l.c_str(), { w, h }); });

    // -- Value editors --
    ui.set_function("checkbox",
                    [](const std::string& l, bool v) -> std::tuple<bool, bool>
                    {
                        auto c = ImGui::Checkbox(l.c_str(), &v);
                        return { v, c };
                    });

    ui.set_function(
        "drag_float",
        [](const std::string& l, float v, float spd, float mn, float mx)
            -> std::tuple<float, bool>
        {
            auto c = ImGui::DragFloat(l.c_str(), &v, spd, mn, mx);
            return { v, c };
        });

    ui.set_function("slider_float",
                    [](const std::string& l, float v, float mn, float mx)
                        -> std::tuple<float, bool>
                    {
                        auto c = ImGui::SliderFloat(l.c_str(), &v, mn, mx);
                        return { v, c };
                    });

    ui.set_function(
        "slider_int",
        [](const std::string& l, int v, int mn, int mx) -> std::tuple<int, bool>
        {
            auto c = ImGui::SliderInt(l.c_str(), &v, mn, mx);
            return { v, c };
        });

    // Uses imgui_stdlib.h: ImGui::InputText(const char*, std::string*, ...)
    ui.set_function("input_text",
                    [](const std::string& label,
                       std::string        text) -> std::tuple<std::string, bool>
                    {
                        auto c = ImGui::InputText(label.c_str(), &text);
                        return { std::move(text), c };
                    });

    ui.set_function("color_edit3",
                    [](const std::string& l, float r, float g, float b)
                        -> std::tuple<float, float, float, bool>
                    {
                        std::array col{ r, g, b };
                        auto       c = ImGui::ColorEdit3(l.c_str(), col.data());
                        return { col[0], col[1], col[2], c };
                    });

    ui.set_function("combo",
                    [](const std::string& label,
                       int                current,
                       sol::table         items) -> std::tuple<int, bool>
                    {
                        auto strs =
                            items | std::views::values |
                            std::views::transform(
                                [](const auto& v)
                                { return v.template as<std::string>(); }) |
                            std::ranges::to<std::vector<std::string>>();

                        auto ptrs = strs |
                                    std::views::transform(&std::string::c_str) |
                                    std::ranges::to<std::vector<const char*>>();

                        auto c = ImGui::Combo(label.c_str(),
                                              &current,
                                              ptrs.data(),
                                              static_cast<int>(ptrs.size()));
                        return { current, c };
                    });

    ui.set_function("progress_bar",
                    [](float fraction, const std::string& overlay)
                    {
                        ImGui::ProgressBar(fraction,
                                           { -1, 0 },
                                           overlay.empty() ? nullptr
                                                           : overlay.c_str());
                    });

    ui.set_function("tooltip",
                    [](const std::string& t)
                    {
                        if (ImGui::IsItemHovered())
                            ImGui::SetTooltip("%s", t.c_str());
                    });

    // -- Tree --
    bind_begin_fn("tree_node",
                  [](const char* s) { return ImGui::TreeNode(s); });
    bind_void_fn("tree_pop", [] { ImGui::TreePop(); });

    // -- Tabs --
    bind_begin_fn("tab_bar_begin",
                  [](const char* s) { return ImGui::BeginTabBar(s); });
    bind_void_fn("tab_bar_end", [] { ImGui::EndTabBar(); });
    bind_begin_fn("tab_item_begin",
                  [](const char* s) { return ImGui::BeginTabItem(s); });
    bind_void_fn("tab_item_end", [] { ImGui::EndTabItem(); });

    // -- Layout --
    bind_void_fn("separator", [] { ImGui::Separator(); });
    bind_void_fn("same_line", [] { ImGui::SameLine(); });
    bind_void_fn("spacing", [] { ImGui::Spacing(); });

    ui.set_function("collapsing_header",
                    [](const std::string& l, bool open) -> bool
                    {
                        return ImGui::CollapsingHeader(
                            l.c_str(),
                            open ? ImGuiTreeNodeFlags_DefaultOpen : 0);
                    });

    ui.set_function(
        "set_next_window_pos",
        [](float x, float y)
        { ImGui::SetNextWindowPos({ x, y }, ImGuiCond_FirstUseEver); });

    ui.set_function(
        "set_next_window_size",
        [](float w, float h)
        { ImGui::SetNextWindowSize({ w, h }, ImGuiCond_FirstUseEver); });

    // -- Cursor / window queries --
    ui.set_function("set_cursor_pos_x",
                    [](float x) { ImGui::SetCursorPosX(x); });
    ui.set_function("get_window_width",
                    []() -> float { return ImGui::GetWindowWidth(); });

    // -- Styling (push/pop for per-widget color overrides) --
    ui.set_function(
        "push_style_color",
        [](int idx, float r, float g, float b, float a)
        { ImGui::PushStyleColor(idx, ImVec4(r, g, b, a)); });

    bind_void_fn("pop_style_color", [] { ImGui::PopStyleColor(); });

    ui.set_function(
        "push_style_var_float",
        [](int idx, float v) { ImGui::PushStyleVar(idx, v); });

    ui.set_function(
        "push_style_var_vec2",
        [](int idx, float x, float y)
        { ImGui::PushStyleVar(idx, ImVec2(x, y)); });

    bind_void_fn("pop_style_var", [] { ImGui::PopStyleVar(); });

    // -- Column / table helpers --
    ui.set_function("columns",
                    [](int count, const std::string& id, bool border)
                    { ImGui::Columns(count, id.c_str(), border); });

    bind_void_fn("next_column", [] { ImGui::NextColumn(); });

    ui.set_function("set_column_width",
                    [](int idx, float w)
                    { ImGui::SetColumnWidth(idx, w); });

    // -- IO queries --
    ui.set_function("get_delta_time",
                    []() -> float { return ImGui::GetIO().DeltaTime; });
    ui.set_function("get_framerate",
                    []() -> float { return ImGui::GetIO().Framerate; });
    ui.set_function("want_capture_keyboard",
                    []() -> bool
                    { return ImGui::GetIO().WantCaptureKeyboard; });
    ui.set_function("want_capture_mouse",
                    []() -> bool { return ImGui::GetIO().WantCaptureMouse; });
}

} // namespace sdk::lua
