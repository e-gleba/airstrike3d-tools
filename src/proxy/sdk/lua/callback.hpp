#include "callback.hpp"

namespace sdk {

void callback_list::add_on_frame(std::function<void()> fn) {
    std::lock_guard lock(mtx);
    on_frame_fns.push_back(std::move(fn));
}

void callback_list::add_on_overlay(std::function<void()> fn) {
    std::lock_guard lock(mtx);
    on_overlay_fns.push_back(std::move(fn));
}

void callback_list::add_on_gl_identity(std::function<void()> fn) {
    std::lock_guard lock(mtx);
    on_gl_identity_fns.push_back(std::move(fn));
}

void callback_list::add_on_glu_lookat(std::function<bool(double, double, double, double, double, double, double, double, double)> fn) {
    std::lock_guard lock(mtx);
    on_glu_lookat_fns.push_back(std::move(fn));
}

void callback_list::add_on_key_down(std::function<bool(int)> fn) {
    std::lock_guard lock(mtx);
    on_key_down_fns.push_back(std::move(fn));
}

void callback_list::add_on_load(std::function<void()> fn) {
    std::lock_guard lock(mtx);
    on_load_fns.push_back(std::move(fn));
}

void callback_list::add_on_unload(std::function<void()> fn) {
    std::lock_guard lock(mtx);
    on_unload_fns.push_back(std::move(fn));
}

void callback_list::invoke_on_frame() {
    std::lock_guard lock(mtx);
    for (auto& fn : on_frame_fns) {
        fn();
    }
}

void callback_list::invoke_on_overlay() {
    std::lock_guard lock(mtx);
    for (auto& fn : on_overlay_fns) {
        fn();
    }
}

void callback_list::invoke_on_gl_identity() {
    std::lock_guard lock(mtx);
    for (auto& fn : on_gl_identity_fns) {
        fn();
    }
}

bool callback_list::invoke_on_glu_lookat(double ex, double ey, double ez, double cx, double cy, double cz, double ux, double uy, double uz) {
    std::lock_guard lock(mtx);
    for (auto& fn : on_glu_lookat_fns) {
        if (fn(ex, ey, ez, cx, cy, cz, ux, uy, uz)) {
            return true; // consumed
        }
    }
    return false;
}

bool callback_list::invoke_on_key_down(int vk) {
    std::lock_guard lock(mtx);
    for (auto& fn : on_key_down_fns) {
        if (fn(vk)) {
            return true; // consumed
        }
    }
    return false;
}

void callback_list::invoke_on_load() {
    std::lock_guard lock(mtx);
    for (auto& fn : on_load_fns) {
        fn();
    }
}

void callback_list::invoke_on_unload() {
    std::lock_guard lock(mtx);
    for (auto& fn : on_unload_fns) {
        fn();
    }
}

void callback_list::clear() {
    std::lock_guard lock(mtx);
    on_frame_fns.clear();
    on_overlay_fns.clear();
    on_gl_identity_fns.clear();
    on_glu_lookat_fns.clear();
    on_key_down_fns.clear();
    on_load_fns.clear();
    on_unload_fns.clear();
}

bool callback_list::empty() const {
    std::lock_guard lock(mtx);
    return on_frame_fns.empty() &&
           on_overlay_fns.empty() &&
           on_gl_identity_fns.empty() &&
           on_glu_lookat_fns.empty() &&
           on_key_down_fns.empty() &&
           on_load_fns.empty() &&
           on_unload_fns.empty();
}

} // namespace sdk
