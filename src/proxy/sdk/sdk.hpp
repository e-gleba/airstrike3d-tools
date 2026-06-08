#pragma once

#include <functional>

namespace sdk {

// Public callback registration API.
// Implementation details (storage, threading, script backend) are hidden.

void on_frame(std::function<void()> callback);
void on_overlay(std::function<void()> callback);
void on_gl_identity(std::function<void()> callback);

void on_glu_lookat(
    std::function<bool(double, double, double, double, double, double, double, double, double)>
        callback);

void on_key_down(std::function<bool(int)> callback);

void on_load(std::function<void()> callback);
void on_unload(std::function<void()> callback);

// Internal dispatch — called by hooks and SDK internals.
namespace detail {

void invoke_on_frame();
void invoke_on_overlay();
void invoke_on_gl_identity();

bool invoke_on_glu_lookat(double ex, double ey, double ez, double cx, double cy, double cz,
                          double ux, double uy, double uz);

bool invoke_on_key_down(int vk);

void invoke_on_load();
void invoke_on_unload();

void clear_all();

} // namespace detail

} // namespace sdk
