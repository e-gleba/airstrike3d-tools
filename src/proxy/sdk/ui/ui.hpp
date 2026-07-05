/// @file ui.hpp
/// @brief Immediate-mode UI widget API (backend-agnostic public interface).

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace sdk::ui
{

[[nodiscard]] bool begin_window(std::string_view title) noexcept;
void end_window() noexcept;

void text(std::string_view t) noexcept;
void text_wrapped(std::string_view t) noexcept;
void text_disabled(std::string_view t) noexcept;
void text_colored(float r, float g, float b, float a, std::string_view t) noexcept;

[[nodiscard]] bool button(std::string_view label) noexcept;
[[nodiscard]] bool button_sized(std::string_view label, float w, float h) noexcept;

[[nodiscard]] bool checkbox(std::string_view label, bool& v) noexcept;
[[nodiscard]] bool drag_float(std::string_view label, float& v,
                              float spd, float mn, float mx) noexcept;
[[nodiscard]] bool slider_float(std::string_view label, float& v,
                                float mn, float mx) noexcept;
[[nodiscard]] bool slider_int(std::string_view label, std::int32_t& v,
                              std::int32_t mn, std::int32_t mx) noexcept;

/// May throw if @p text reallocation fails while editing.
[[nodiscard]] bool input_text(std::string_view label, std::string& text);

[[nodiscard]] bool color_edit3(std::string_view label,
                               float& r, float& g, float& b) noexcept;

void separator() noexcept;
void same_line() noexcept;
void spacing() noexcept;
[[nodiscard]] bool tree_node(std::string_view label) noexcept;
void tree_pop() noexcept;
[[nodiscard]] bool tab_bar_begin(std::string_view label) noexcept;
void tab_bar_end() noexcept;
[[nodiscard]] bool tab_item_begin(std::string_view label) noexcept;
void tab_item_end() noexcept;
[[nodiscard]] bool collapsing_header(std::string_view label, bool open) noexcept;

void begin_group() noexcept;
void end_group() noexcept;

void set_next_window_pos(float x, float y) noexcept;
void set_next_window_size(float w, float h) noexcept;
void set_cursor_pos_x(float x) noexcept;
[[nodiscard]] float get_window_width() noexcept;

void push_style_color(std::int32_t idx, float r, float g, float b, float a) noexcept;
void pop_style_color() noexcept;
void push_style_var_float(std::int32_t idx, float v) noexcept;
void push_style_var_vec2(std::int32_t idx, float x, float y) noexcept;
void pop_style_var() noexcept;

void columns(std::int32_t count, std::string_view id, bool border) noexcept;
void next_column() noexcept;
void set_column_width(std::int32_t idx, float w) noexcept;

[[nodiscard]] float get_delta_time() noexcept;
[[nodiscard]] float get_framerate() noexcept;
[[nodiscard]] bool want_capture_keyboard() noexcept;
[[nodiscard]] bool want_capture_mouse() noexcept;
void progress_bar(float fraction, std::string_view overlay) noexcept;
void tooltip(std::string_view t) noexcept;

} // namespace sdk::ui
