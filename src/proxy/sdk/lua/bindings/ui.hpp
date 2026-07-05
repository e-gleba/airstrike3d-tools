/// @file bindings/ui.hpp
/// @brief Pure C++ UI functions for Lua binding (no sol2 types).

#pragma once

#include <cstdint>
#include <string>

namespace sdk::lua::bindings::ui
{

// ── Window management ────────────────────────────────────────────────────────

[[nodiscard]] bool begin_window(const std::string& title) noexcept;
void end_window() noexcept;

// ── Text rendering ───────────────────────────────────────────────────────────

void text(const std::string& t) noexcept;
void text_wrapped(const std::string& t) noexcept;
void text_disabled(const std::string& t) noexcept;
void text_colored(float r, float g, float b, float a,
                  const std::string& t) noexcept;

// ── Buttons ──────────────────────────────────────────────────────────────────

[[nodiscard]] bool button(const std::string& label) noexcept;
[[nodiscard]] bool button_sized(const std::string& label,
                                float w, float h) noexcept;

// ── Input widgets ────────────────────────────────────────────────────────────

[[nodiscard]] bool checkbox(const std::string& label, bool& v) noexcept;
[[nodiscard]] bool drag_float(const std::string& label, float& v,
                              float spd, float mn, float mx) noexcept;
[[nodiscard]] bool slider_float(const std::string& label, float& v,
                                float mn, float mx) noexcept;
[[nodiscard]] bool slider_int(const std::string& label, std::int32_t& v,
                              std::int32_t mn, std::int32_t mx) noexcept;
[[nodiscard]] bool input_text(const std::string& label,
                              std::string& text) noexcept;
[[nodiscard]] bool color_edit3(const std::string& label,
                               float& r, float& g, float& b) noexcept;

// ── Layout ───────────────────────────────────────────────────────────────────

void separator() noexcept;
void same_line() noexcept;
void spacing() noexcept;
[[nodiscard]] bool tree_node(const std::string& label) noexcept;
void tree_pop() noexcept;
[[nodiscard]] bool tab_bar_begin(const std::string& label) noexcept;
void tab_bar_end() noexcept;
[[nodiscard]] bool tab_item_begin(const std::string& label) noexcept;
void tab_item_end() noexcept;
[[nodiscard]] bool collapsing_header(const std::string& label,
                                     bool open) noexcept;

// ── Groups ───────────────────────────────────────────────────────────────────

void begin_group() noexcept;
void end_group() noexcept;

// ── Positioning ──────────────────────────────────────────────────────────────

void set_next_window_pos(float x, float y) noexcept;
void set_next_window_size(float w, float h) noexcept;
void set_cursor_pos_x(float x) noexcept;
[[nodiscard]] float get_window_width() noexcept;

// ── Styling ──────────────────────────────────────────────────────────────────

void push_style_color(std::int32_t idx, float r, float g, float b,
                      float a) noexcept;
void pop_style_color() noexcept;
void push_style_var_float(std::int32_t idx, float v) noexcept;
void push_style_var_vec2(std::int32_t idx, float x, float y) noexcept;
void pop_style_var() noexcept;

// ── Columns ──────────────────────────────────────────────────────────────────

void columns(std::int32_t count, const std::string& id, bool border) noexcept;
void next_column() noexcept;
void set_column_width(std::int32_t idx, float w) noexcept;

// ── Utilities ────────────────────────────────────────────────────────────────

[[nodiscard]] float get_delta_time() noexcept;
[[nodiscard]] float get_framerate() noexcept;
[[nodiscard]] bool want_capture_keyboard() noexcept;
[[nodiscard]] bool want_capture_mouse() noexcept;
void progress_bar(float fraction, const std::string& overlay) noexcept;
void tooltip(const std::string& t) noexcept;

} // namespace sdk::lua::bindings::ui
