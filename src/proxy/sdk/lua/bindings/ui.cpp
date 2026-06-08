// src/proxy/sdk/lua/bindings/ui.cpp
// ImGui UI functions exposed to Lua.
// Pure C++ — no sol2 types.

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <array>

namespace sdk::lua::bindings::ui
{

// ── Window management ────────────────────────────────────────────────────────

bool begin_window(const std::string& title) noexcept
{
    return ImGui::Begin(title.c_str());
}

void end_window() noexcept
{
    ImGui::End();
}

// ── Text rendering ───────────────────────────────────────────────────────────

void text(const std::string& t) noexcept
{
    ImGui::Text("%s", t.c_str());
}

void text_wrapped(const std::string& t) noexcept
{
    ImGui::TextWrapped("%s", t.c_str());
}

void text_disabled(const std::string& t) noexcept
{
    ImGui::TextDisabled("%s", t.c_str());
}

void text_colored(float r, float g, float b, float a,
                  const std::string& t) noexcept
{
    ImGui::TextColored({r, g, b, a}, "%s", t.c_str());
}

// ── Buttons ──────────────────────────────────────────────────────────────────

bool button(const std::string& label) noexcept
{
    return ImGui::Button(label.c_str(), {-1, 0});
}

bool button_sized(const std::string& label, float w, float h) noexcept
{
    return ImGui::Button(label.c_str(), {w, h});
}

// ── Input widgets ────────────────────────────────────────────────────────────

bool checkbox(const std::string& label, bool& v) noexcept
{
    return ImGui::Checkbox(label.c_str(), &v);
}

bool drag_float(const std::string& label, float& v,
                float spd, float mn, float mx) noexcept
{
    return ImGui::DragFloat(label.c_str(), &v, spd, mn, mx);
}

bool slider_float(const std::string& label, float& v,
                  float mn, float mx) noexcept
{
    return ImGui::SliderFloat(label.c_str(), &v, mn, mx);
}

bool slider_int(const std::string& label, int& v, int mn, int mx) noexcept
{
    return ImGui::SliderInt(label.c_str(), &v, mn, mx);
}

bool input_text(const std::string& label, std::string& text) noexcept
{
    return ImGui::InputText(label.c_str(), &text);
}

bool color_edit3(const std::string& label, float& r, float& g,
                 float& b) noexcept
{
    std::array col{r, g, b};
    auto changed = ImGui::ColorEdit3(label.c_str(), col.data());
    r = col[0];
    g = col[1];
    b = col[2];
    return changed;
}

// ── Layout ───────────────────────────────────────────────────────────────────

void separator() noexcept  { ImGui::Separator(); }
void same_line() noexcept  { ImGui::SameLine(); }
void spacing() noexcept    { ImGui::Spacing(); }

bool tree_node(const std::string& label) noexcept
{
    return ImGui::TreeNode(label.c_str());
}

void tree_pop() noexcept   { ImGui::TreePop(); }

bool tab_bar_begin(const std::string& label) noexcept
{
    return ImGui::BeginTabBar(label.c_str());
}

void tab_bar_end() noexcept { ImGui::EndTabBar(); }

bool tab_item_begin(const std::string& label) noexcept
{
    return ImGui::BeginTabItem(label.c_str());
}

void tab_item_end() noexcept { ImGui::EndTabItem(); }

bool collapsing_header(const std::string& label, bool open) noexcept
{
    return ImGui::CollapsingHeader(
        label.c_str(), open ? ImGuiTreeNodeFlags_DefaultOpen : 0);
}

// ── Groups ───────────────────────────────────────────────────────────────────

void begin_group() noexcept { ImGui::BeginGroup(); }
void end_group() noexcept   { ImGui::EndGroup(); }

// ── Positioning ──────────────────────────────────────────────────────────────

void set_next_window_pos(float x, float y) noexcept
{
    ImGui::SetNextWindowPos({x, y}, ImGuiCond_FirstUseEver);
}

void set_next_window_size(float w, float h) noexcept
{
    ImGui::SetNextWindowSize({w, h}, ImGuiCond_FirstUseEver);
}

void set_cursor_pos_x(float x) noexcept
{
    ImGui::SetCursorPosX(x);
}

float get_window_width() noexcept
{
    return ImGui::GetWindowWidth();
}

// ── Styling ──────────────────────────────────────────────────────────────────

void push_style_color(int idx, float r, float g, float b, float a) noexcept
{
    ImGui::PushStyleColor(idx, ImVec4(r, g, b, a));
}

void pop_style_color() noexcept { ImGui::PopStyleColor(); }

void push_style_var_float(int idx, float v) noexcept
{
    ImGui::PushStyleVar(idx, v);
}

void push_style_var_vec2(int idx, float x, float y) noexcept
{
    ImGui::PushStyleVar(idx, ImVec2(x, y));
}

void pop_style_var() noexcept { ImGui::PopStyleVar(); }

// ── Columns ──────────────────────────────────────────────────────────────────

void columns(int count, const std::string& id, bool border) noexcept
{
    ImGui::Columns(count, id.c_str(), border);
}

void next_column() noexcept { ImGui::NextColumn(); }

void set_column_width(int idx, float w) noexcept
{
    ImGui::SetColumnWidth(idx, w);
}

// ── Utilities ────────────────────────────────────────────────────────────────

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

void progress_bar(float fraction, const std::string& overlay) noexcept
{
    ImGui::ProgressBar(fraction, {-1, 0},
                       overlay.empty() ? nullptr : overlay.c_str());
}

void tooltip(const std::string& t) noexcept
{
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("%s", t.c_str());
    }
}

} // namespace sdk::lua::bindings::ui
