/// @file bindings/ui.hpp
/// @brief Pure C++ UI functions for scripting (no backend types).
///
/// All functions use simple types — no ImGui types in the public API.

#pragma once

#include <string>

namespace sdk::scripting::bindings::ui
{

// ── Window management ───────────────────────────────────────────────────────

/// Begin a new window. Returns true if window is visible.
[[nodiscard]] bool begin_window(const std::string& title) noexcept;

/// End the current window.
void end_window() noexcept;

// ── Text rendering ──────────────────────────────────────────────────────────

void text(const std::string& t) noexcept;
void text_wrapped(const std::string& t) noexcept;
void text_disabled(const std::string& t) noexcept;
void text_colored(float r, float g, float b, float a,
                  const std::string& t) noexcept;

// ── Buttons ─────────────────────────────────────────────────────────────────

/// Render a button. Returns true if clicked.
[[nodiscard]] bool button(const std::string& label) noexcept;

/// Render a button with fixed size. Returns true if clicked.
[[nodiscard]] bool button_sized(const std::string& label,
                                float w, float h) noexcept;

// ── Input widgets ───────────────────────────────────────────────────────────

/// Render a checkbox. Returns true if value changed.
[[nodiscard]] bool checkbox(const std::string& label, bool& v) noexcept;

/// Render a drag float slider. Returns true if value changed.
[[nodiscard]] bool drag_float(const std::string& label, float& v,
                              float spd, float mn, float mx) noexcept;

/// Render a slider float. Returns true if value changed.
[[nodiscard]] bool slider_float(const std::string& label, float& v,
                                float mn, float mx) noexcept;

/// Render a slider int. Returns true if value changed.
[[nodiscard]] bool slider_int(const std::string& label, int& v,
                              int mn, int mx) noexcept;

/// Render a text input. Returns true if value changed.
[[nodiscard]] bool input_text(const std::string& label,
                              std::string& text) noexcept;

/// Render a color editor (RGB). Returns true if value changed.
[[nodiscard]] bool color_edit3(const std::string& label,
                               float& r, float& g, float& b) noexcept;

// ── Layout ──────────────────────────────────────────────────────────────────

void separator() noexcept;
void same_line() noexcept;
void spacing() noexcept;

/// Begin a tree node. Returns true if node is open.
[[nodiscard]] bool tree_node(const std::string& label) noexcept;

/// End a tree node.
void tree_pop() noexcept;

/// Begin a tab bar. Returns true if successful.
[[nodiscard]] bool tab_bar_begin(const std::string& label) noexcept;

/// End a tab bar.
void tab_bar_end() noexcept;

/// Begin a tab item. Returns true if tab is selected.
[[nodiscard]] bool tab_item_begin(const std::string& label) noexcept;

/// End a tab item.
void tab_item_end() noexcept;

/// Render a collapsing header. Returns true if open.
[[nodiscard]] bool collapsing_header(const std::string& label,
                                     bool open) noexcept;

// ── Groups ──────────────────────────────────────────────────────────────────

void begin_group() noexcept;
void end_group() noexcept;

// ── Positioning ─────────────────────────────────────────────────────────────

void set_next_window_pos(float x, float y) noexcept;
void set_next_window_size(float w, float h) noexcept;
void set_cursor_pos_x(float x) noexcept;

/// Get current window width.
[[nodiscard]] float get_window_width() noexcept;

// ── Styling ─────────────────────────────────────────────────────────────────

void push_style_color(int idx, float r, float g, float b, float a) noexcept;
void pop_style_color() noexcept;

void push_style_var_float(int idx, float v) noexcept;
void push_style_var_vec2(int idx, float x, float y) noexcept;
void pop_style_var() noexcept;

// ── Columns ─────────────────────────────────────────────────────────────────

void columns(int count, const std::string& id, bool border) noexcept;
void next_column() noexcept;
void set_column_width(int idx, float w) noexcept;

// ── Utilities ───────────────────────────────────────────────────────────────

/// Get time since last frame (seconds).
[[nodiscard]] float get_delta_time() noexcept;

/// Get current framerate (FPS).
[[nodiscard]] float get_framerate() noexcept;

/// Check if UI wants to capture keyboard input.
[[nodiscard]] bool want_capture_keyboard() noexcept;

/// Check if UI wants to capture mouse input.
[[nodiscard]] bool want_capture_mouse() noexcept;

/// Render a progress bar.
void progress_bar(float fraction, const std::string& overlay) noexcept;

/// Show a tooltip (must be called after the item).
void tooltip(const std::string& t) noexcept;

} // namespace sdk::scripting::bindings::ui
