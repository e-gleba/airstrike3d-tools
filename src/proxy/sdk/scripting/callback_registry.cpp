// sdk/scripting/callback_registry.cpp — Callback registry implementation

#include "sdk/scripting/callback_registry.hpp"

namespace sdk::scripting
{

callback_registry::callback_registry() = default;

void callback_registry::clear_all()
{
    on_frame_.clear();
    on_overlay_.clear();
    on_gl_identity_.clear();
    on_glu_lookat_.clear();
    on_load_.clear();
    on_unload_.clear();
    on_key_down_.clear();
}

} // namespace sdk::scripting
