// src/proxy/sdk/scripting/bindings/ui_imgui.cpp
// ImGui UI functions exposed to scripting.
// Implementation uses ImGui internally, but public API is clean.

#include "ui.hpp"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <sol/sol.hpp>

#include <array>

namespace sdk::scripting::bindings::ui
{

// ── Window management ───────────────────────────────────────────────────────

bool begin_window(const std::string& title) noexcept
{
    return ImGui::Begin(title.c_str());
}

void end_window() noexcept
{
    ImGui::End();
}

// ── Text rendering ──────────────────────────────────────────────────────────

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

// ── Buttons ─────────────────────────────────────────────────────────────────

bool button(const std::string& label) noexcept
{
    return ImGui::Button(label.c_str(), {-1, 0});
}

bool button_sized(const std::string& label, float w, float h) noexcept
{
    return ImGui::Button(label.c_str(), {w, h});
}

// ── Input widgets ───────────────────────────────────────────────────────────

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

// ── Layout ──────────────────────────────────────────────────────────────────

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

// ── Groups ──────────────────────────────────────────────────────────────────

void begin_group() noexcept { ImGui::BeginGroup(); }
void end_group() noexcept   { ImGui::EndGroup(); }

// ── Positioning ─────────────────────────────────────────────────────────────

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

// ── Styling ─────────────────────────────────────────────────────────────────

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

// ── Columns ─────────────────────────────────────────────────────────────────

void columns(int count, const std::string& id, bool border) noexcept
{
    ImGui::Columns(count, id.c_str(), border);
}

void next_column() noexcept { ImGui::NextColumn(); }

void set_column_width(int idx, float w) noexcept
{
    ImGui::SetColumnWidth(idx, w);
}

// ── Utilities ───────────────────────────────────────────────────────────────

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

// ── Registration function for Lua bindings ──────────────────────────────────

void register_ui(sol::state& lua)
{
    auto ui = lua["ui"].get_or_create<sol::table>();
    
    // Window management
    ui["begin_window"] = &begin_window;
    ui["end_window"] = &end_window;
    
    // Text rendering
    ui["text"] = &text;
    ui["text_wrapped"] = &text_wrapped;
    ui["text_disabled"] = &text_disabled;
    ui["text_colored"] = &text_colored;
    
    // Buttons
    ui["button"] = &button;
    ui["button_sized"] = &button_sized;
    
    // Input widgets
    ui["checkbox"] = [](const std::string& label, bool v) {
        bool result = checkbox(label, v);
        return std::make_tuple(v, result);
    };
    ui["drag_float"] = [](const std::string& label, float v,
                          float spd, float mn, float mx) {
        bool result = drag_float(label, v, spd, mn, mx);
        return std::make_tuple(v, result);
    };
    ui["slider_float"] = [](const std::string& label, float v,
                            float mn, float mx) {
        bool result = slider_float(label, v, mn, mx);
        return std::make_tuple(v, result);
    };
    ui["slider_int"] = [](const std::string& label, int v,
                          int mn, int mx) {
        bool result = slider_int(label, v, mn, mx);
        return std::make_tuple(v, result);
    };
    ui["input_text"] = [](const std::string& label, std::string text) {
        bool result = input_text(label, text);
        return std::make_tuple(text, result);
    };
    ui["color_edit3"] = [](const std::string& label, float r, float g, float b) {
        bool result = color_edit3(label, r, g, b);
        return std::make_tuple(r, g, b, result);
    };
    
    // Layout
    ui["separator"] = &separator;
    ui["same_line"] = &same_line;
    ui["spacing"] = &spacing;
    ui["tree_node"] = &tree_node;
    ui["tree_pop"] = &tree_pop;
    ui["tab_bar_begin"] = &tab_bar_begin;
    ui["tab_bar_end"] = &tab_bar_end;
    ui["tab_item_begin"] = &tab_item_begin;
    ui["tab_item_end"] = &tab_item_end;
    ui["collapsing_header"] = &collapsing_header;
    
    // Groups
    ui["begin_group"] = &begin_group;
    ui["end_group"] = &end_group;
    
    // Positioning
    ui["set_next_window_pos"] = &set_next_window_pos;
    ui["set_next_window_size"] = &set_next_window_size;
    ui["set_cursor_pos_x"] = &set_cursor_pos_x;
    ui["get_window_width"] = &get_window_width;
    
    // Styling
    ui["push_style_color"] = &push_style_color;
    ui["pop_style_color"] = &pop_style_color;
    ui["push_style_var_float"] = &push_style_var_float;
    ui["push_style_var_vec2"] = &push_style_var_vec2;
    ui["pop_style_var"] = &pop_style_var;
    
    // Columns
    ui["columns"] = &columns;
    ui["next_column"] = &next_column;
    ui["set_column_width"] = &set_column_width;
    
    // Utilities
    ui["get_delta_time"] = &get_delta_time;
    ui["get_framerate"] = &get_framerate;
    ui["want_capture_keyboard"] = &want_capture_keyboard;
    ui["want_capture_mouse"] = &want_capture_mouse;
    ui["progress_bar"] = &progress_bar;
    ui["tooltip"] = &tooltip;
}

} // namespace sdk::scripting::bindings::ui
