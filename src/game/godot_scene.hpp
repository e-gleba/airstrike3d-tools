#pragma once

#include <string>
#include <filesystem>

namespace godot_scene
{

/// Load a Godot TSCN scene file
/// Resolves res:// paths relative to the TSCN file's directory
/// @param tscn_path Path to the .tscn file (can be absolute or relative)
/// @return true if at least one object was loaded successfully
bool load_tscn(const std::string& tscn_path);

/// Resolve a Godot resource path (res:// or relative) to an absolute filesystem path
/// @param path The path from TSCN (may be res://, relative, or absolute)
/// @param tscn_dir The directory containing the TSCN file (used as base for res://)
/// @return Resolved absolute path
std::filesystem::path resolve_resource_path(const std::string& path, const std::filesystem::path& tscn_dir);

} // namespace godot_scene

