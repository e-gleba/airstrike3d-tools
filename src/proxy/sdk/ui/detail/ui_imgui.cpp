#include "sdk/ui/ui.hpp"

#include "sdk/ui/detail/cstr_buffer.hpp"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <array>

namespace sdk::ui
{

bool begin_window(std::string_view title) noexcept
{
    const detail::cstr_buffer label{ title };
    return ImGui::Begin(label.c_str());
}

void end_window() noexcept
{
    ImGui::End();
}

void text(std::string_view t) noexcept
{
    ImGui::TextUnformatted(t.data(), t.data() + t.size());
}

void text_wrapped(std::string_view t) noexcept
{
    const detail::cstr_buffer content{ t };
    ImGui::TextWrapped("%s", content.c_str());
}

void text_disabled(std::string_view t) noexcept
{
    const detail::cstr_buffer content{ t };
    ImGui::TextDisabled("%s", content.c_str());
}

void text_colored(float r, float g, float b, float a, std::string_view t) noexcept
{
    const detail::cstr_buffer content{ t };
    ImGui::TextColored({ r, g, b, a }, "%s", content.c_str());
}

bool button(std::string_view label) noexcept
{
    const detail::cstr_buffer text{ label };
    return ImGui::Button(text.c_str(), { -1, 0 });
}

bool button_sized(std::string_view label, float w, float h) noexcept
{
    const detail::cstr_buffer text{ label };
    return ImGui::Button(text.c_str(), { w, h });
}

bool checkbox(std::string_view label, bool& v) noexcept
{
    const detail::cstr_buffer text{ label };
    return ImGui::Checkbox(text.c_str(), &v);
}

bool drag_float(std::string_view label, float& v, float spd, float mn, float mx) noexcept
{
    const detail::cstr_buffer text{ label };
    return ImGui::DragFloat(text.c_str(), &v, spd, mn, mx);
}

bool slider_float(std::string_view label, float& v, float mn, float mx) noexcept
{
    const detail::cstr_buffer text{ label };
    return ImGui::SliderFloat(text.c_str(), &v, mn, mx);
}

bool slider_int(std::string_view label, std::int32_t& v, std::int32_t mn, std::int32_t mx) noexcept
{
    const detail::cstr_buffer text{ label };
    return ImGui::SliderInt(text.c_str(), &v, mn, mx);
}

bool input_text(std::string_view label, std::string& text)
{
    const detail::cstr_buffer id{ label };
    return ImGui::InputText(id.c_str(), &text);
}

bool color_edit3(std::string_view label, float& r, float& g, float& b) noexcept
{
    const detail::cstr_buffer id{ label };
    std::array col{ r, g, b };
    const auto changed = ImGui::ColorEdit3(id.c_str(), col.data());
    r = col[0];
    g = col[1];
    b = col[2];
    return changed;
}

void separator() noexcept { ImGui::Separator(); }
void same_line() noexcept { ImGui::SameLine(); }
void spacing() noexcept { ImGui::Spacing(); }

bool tree_node(std::string_view label) noexcept
{
    const detail::cstr_buffer text{ label };
    return ImGui::TreeNode(text.c_str());
}

void tree_pop() noexcept { ImGui::TreePop(); }

bool tab_bar_begin(std::string_view label) noexcept
{
    const detail::cstr_buffer text{ label };
    return ImGui::BeginTabBar(text.c_str());
}

void tab_bar_end() noexcept { ImGui::EndTabBar(); }

bool tab_item_begin(std::string_view label) noexcept
{
    const detail::cstr_buffer text{ label };
    return ImGui::BeginTabItem(text.c_str());
}

void tab_item_end() noexcept { ImGui::EndTabItem(); }

bool collapsing_header(std::string_view label, bool open) noexcept
{
    const detail::cstr_buffer text{ label };
    return ImGui::CollapsingHeader(
        text.c_str(),
        open ? ImGuiTreeNodeFlags_DefaultOpen : 0);
}

void begin_group() noexcept { ImGui::BeginGroup(); }
void end_group() noexcept { ImGui::EndGroup(); }

void set_next_window_pos(float x, float y) noexcept
{
    ImGui::SetNextWindowPos({ x, y }, ImGuiCond_FirstUseEver);
}

void set_next_window_size(float w, float h) noexcept
{
    ImGui::SetNextWindowSize({ w, h }, ImGuiCond_FirstUseEver);
}

void set_cursor_pos_x(float x) noexcept
{
    ImGui::SetCursorPosX(x);
}

float get_window_width() noexcept
{
    return ImGui::GetWindowWidth();
}

void push_style_color(std::int32_t idx, float r, float g, float b, float a) noexcept
{
    ImGui::PushStyleColor(idx, ImVec4{ r, g, b, a });
}

void pop_style_color() noexcept { ImGui::PopStyleColor(); }

void push_style_var_float(std::int32_t idx, float v) noexcept
{
    ImGui::PushStyleVar(idx, v);
}

void push_style_var_vec2(std::int32_t idx, float x, float y) noexcept
{
    ImGui::PushStyleVar(idx, ImVec2{ x, y });
}

void pop_style_var() noexcept { ImGui::PopStyleVar(); }

void columns(std::int32_t count, std::string_view id, bool border) noexcept
{
    const detail::cstr_buffer name{ id };
    ImGui::Columns(count, name.c_str(), border);
}

void next_column() noexcept { ImGui::NextColumn(); }

void set_column_width(std::int32_t idx, float w) noexcept
{
    ImGui::SetColumnWidth(idx, w);
}

float get_delta_time() noexcept
{
    return ImGui::GetIO().DeltaTime;
}

float get_framerate() noexcept
{
    return ImGui::GetIO().Framerate;
}

bool want_capture_keyboard() noexcept
{
    return ImGui::GetIO().WantCaptureKeyboard;
}

bool want_capture_mouse() noexcept
{
    return ImGui::GetIO().WantCaptureMouse;
}

void progress_bar(float fraction, std::string_view overlay) noexcept
{
    const detail::cstr_buffer text{ overlay };
    ImGui::ProgressBar(fraction,
                       { -1, 0 },
                       overlay.empty() ? nullptr : text.c_str());
}

void tooltip(std::string_view t) noexcept
{
    if (ImGui::IsItemHovered())
    {
        const detail::cstr_buffer text{ t };
        ImGui::SetTooltip("%s", text.c_str());
    }
}

} // namespace sdk::ui
