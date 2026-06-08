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

} // namespace sdk
