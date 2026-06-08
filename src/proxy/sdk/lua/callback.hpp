#pragma once

#include <functional>
#include <mutex>
#include <vector>

namespace sdk
{

/// Type-safe callback registry for SDK events.
///
/// Thread-safe via recursive_mutex. Callbacks stored as std::function.
/// Consuming callbacks (on_key_down, on_glu_lookat) return bool to indicate consumption.
class callback_list
{
    std::recursive_mutex& mtx;

    std::vector<std::function<void()>>
        on_frame_fns,
        on_overlay_fns,
        on_gl_identity_fns,
        on_load_fns,
        on_unload_fns;

    std::vector<std::function<bool(double, double, double, double, double, double, double, double, double)>>
        on_glu_lookat_fns;

    std::vector<std::function<bool(int)>> on_key_down_fns;

public:
    explicit callback_list(std::recursive_mutex& m) noexcept : mtx{ m }
    {
    }

    void add_on_frame(std::function<void()> fn);
    void add_on_overlay(std::function<void()> fn);
    void add_on_gl_identity(std::function<void()> fn);
    void add_on_glu_lookat(
        std::function<bool(double, double, double, double, double, double, double, double, double)>
            fn);
    void add_on_key_down(std::function<bool(int)> fn);
    void add_on_load(std::function<void()> fn);
    void add_on_unload(std::function<void()> fn);

    void invoke_on_frame();
    void invoke_on_overlay();
    void invoke_on_gl_identity();
    void invoke_on_load();
    void invoke_on_unload();

    [[nodiscard]] bool invoke_on_glu_lookat(double ex, double ey, double ez,
                                            double cx, double cy, double cz,
                                            double ux, double uy, double uz);
    [[nodiscard]] bool invoke_on_key_down(int vk);

    void clear();
    [[nodiscard]] bool empty() const;
};

} // namespace sdk
