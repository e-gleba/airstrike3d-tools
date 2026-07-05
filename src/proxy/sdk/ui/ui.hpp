/// @file ui.hpp
/// @brief Immediate-mode UI widget API (backend-agnostic public interface).

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace sdk::ui
{

[[nodiscard]] bool begin_window(std::string_view title);
void end_window();

void text(std::string_view t);
void text_wrapped(std::string_view t);
void text_disabled(std::string_view t);
void text_colored(float r, float g, float b, float a, std::string_view t);

[[nodiscard]] bool button(std::string_view label);
[[nodiscard]] bool button_sized(std::string_view label, float w, float h);

[[nodiscard]] bool checkbox(std::string_view label, bool& v);
[[nodiscard]] bool drag_float(std::string_view label, float& v,
                              float spd, float mn, float mx);
[[nodiscard]] bool slider_float(std::string_view label, float& v,
                                float mn, float mx);
[[nodiscard]] bool slider_int(std::string_view label, std::int32_t& v,
                              std::int32_t mn, std::int32_t mx);
[[nodiscard]] bool input_text(std::string_view label, std::string& text);
[[nodiscard]] bool color_edit3(std::string_view label,
                               float& r, float& g, float& b);

void separator();
void same_line();
void spacing();
[[nodiscard]] bool tree_node(std::string_view label);
void tree_pop();
[[nodiscard]] bool tab_bar_begin(std::string_view label);
void tab_bar_end();
[[nodiscard]] bool tab_item_begin(std::string_view label);
void tab_item_end();
[[nodiscard]] bool collapsing_header(std::string_view label, bool open);

void begin_group();
void end_group();

void set_next_window_pos(float x, float y);
void set_next_window_size(float w, float h);
void set_cursor_pos_x(float x);
[[nodiscard]] float get_window_width();

void push_style_color(std::int32_t idx, float r, float g, float b, float a);
void pop_style_color();
void push_style_var_float(std::int32_t idx, float v);
void push_style_var_vec2(std::int32_t idx, float x, float y);
void pop_style_var();

void columns(std::int32_t count, std::string_view id, bool border);
void next_column();
void set_column_width(std::int32_t idx, float w);

[[nodiscard]] float get_delta_time();
[[nodiscard]] float get_framerate();
[[nodiscard]] bool want_capture_keyboard();
[[nodiscard]] bool want_capture_mouse();
void progress_bar(float fraction, std::string_view overlay);
void tooltip(std::string_view t);

} // namespace sdk::ui
