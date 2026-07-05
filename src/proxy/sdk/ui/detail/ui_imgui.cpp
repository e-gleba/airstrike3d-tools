#include "sdk/ui/ui.hpp"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <array>
#include <string>

namespace sdk::ui
{

bool begin_window(std::string_view title)
{
    return ImGui::Begin(std::string{ title }.c_str());
}

void end_window()
{
    ImGui::End();
}

void text(std::string_view t)
{
    ImGui::Text("%s", std::string{ t }.c_str());
}

void text_wrapped(std::string_view t)
{
    ImGui::TextWrapped("%s", std::string{ t }.c_str());
}

void text_disabled(std::string_view t)
{
    ImGui::TextDisabled("%s", std::string{ t }.c_str());
}

void text_colored(float r, float g, float b, float a, std::string_view t)
{
    ImGui::TextColored({ r, g, b, a }, "%s", std::string{ t }.c_str());
}

bool button(std::string_view label)
{
    return ImGui::Button(std::string{ label }.c_str(), { -1, 0 });
}

bool button_sized(std::string_view label, float w, float h)
{
    return ImGui::Button(std::string{ label }.c_str(), { w, h });
}

bool checkbox(std::string_view label, bool& v)
{
    return ImGui::Checkbox(std::string{ label }.c_str(), &v);
}

bool drag_float(std::string_view label, float& v, float spd, float mn, float mx)
{
    return ImGui::DragFloat(std::string{ label }.c_str(), &v, spd, mn, mx);
}

bool slider_float(std::string_view label, float& v, float mn, float mx)
{
    return ImGui::SliderFloat(std::string{ label }.c_str(), &v, mn, mx);
}

bool slider_int(std::string_view label, std::int32_t& v, std::int32_t mn, std::int32_t mx)
{
    return ImGui::SliderInt(std::string{ label }.c_str(), &v, mn, mx);
}

bool input_text(std::string_view label, std::string& text)
{
    return ImGui::InputText(std::string{ label }.c_str(), &text);
}

bool color_edit3(std::string_view label, float& r, float& g, float& b)
{
    std::array col{ r, g, b };
    const auto changed = ImGui::ColorEdit3(std::string{ label }.c_str(), col.data());
    r = col[0];
    g = col[1];
    b = col[2];
    return changed;
}

void separator() { ImGui::Separator(); }
void same_line() { ImGui::SameLine(); }
void spacing()   { ImGui::Spacing(); }

bool tree_node(std::string_view label)
{
    return ImGui::TreeNode(std::string{ label }.c_str());
}

void tree_pop() { ImGui::TreePop(); }

bool tab_bar_begin(std::string_view label)
{
    return ImGui::BeginTabBar(std::string{ label }.c_str());
}

void tab_bar_end() { ImGui::EndTabBar(); }

bool tab_item_begin(std::string_view label)
{
    return ImGui::BeginTabItem(std::string{ label }.c_str());
}

void tab_item_end() { ImGui::EndTabItem(); }

bool collapsing_header(std::string_view label, bool open)
{
    return ImGui::CollapsingHeader(
        std::string{ label }.c_str(),
        open ? ImGuiTreeNodeFlags_DefaultOpen : 0);
}

void begin_group() { ImGui::BeginGroup(); }
void end_group()   { ImGui::EndGroup(); }

void set_next_window_pos(float x, float y)
{
    ImGui::SetNextWindowPos({ x, y }, ImGuiCond_FirstUseEver);
}

void set_next_window_size(float w, float h)
{
    ImGui::SetNextWindowSize({ w, h }, ImGuiCond_FirstUseEver);
}

void set_cursor_pos_x(float x)
{
    ImGui::SetCursorPosX(x);
}

float get_window_width()
{
    return ImGui::GetWindowWidth();
}

void push_style_color(std::int32_t idx, float r, float g, float b, float a)
{
    ImGui::PushStyleColor(idx, ImVec4{ r, g, b, a });
}

void pop_style_color() { ImGui::PopStyleColor(); }

void push_style_var_float(std::int32_t idx, float v)
{
    ImGui::PushStyleVar(idx, v);
}

void push_style_var_vec2(std::int32_t idx, float x, float y)
{
    ImGui::PushStyleVar(idx, ImVec2{ x, y });
}

void pop_style_var() { ImGui::PopStyleVar(); }

void columns(std::int32_t count, std::string_view id, bool border)
{
    ImGui::Columns(count, std::string{ id }.c_str(), border);
}

void next_column() { ImGui::NextColumn(); }

void set_column_width(std::int32_t idx, float w)
{
    ImGui::SetColumnWidth(idx, w);
}

float get_delta_time()
{
    return ImGui::GetIO().DeltaTime;
}

float get_framerate()
{
    return ImGui::GetIO().Framerate;
}

bool want_capture_keyboard()
{
    return ImGui::GetIO().WantCaptureKeyboard;
}

bool want_capture_mouse()
{
    return ImGui::GetIO().WantCaptureMouse;
}

void progress_bar(float fraction, std::string_view overlay)
{
    ImGui::ProgressBar(fraction,
                       { -1, 0 },
                       overlay.empty() ? nullptr : std::string{ overlay }.c_str());
}

void tooltip(std::string_view t)
{
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("%s", std::string{ t }.c_str());
    }
}

} // namespace sdk::ui
