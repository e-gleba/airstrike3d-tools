#!/usr/bin/env bash

if [ "$#" -lt 1 ]; then
  echo "usage: $0 <exe_or_bat_path> [args...]"
  exit 1
fi

exe_path="$(realpath "$1")"
exe_name="$(basename "$exe_path")"
exe_dir="$(dirname "$exe_path")"
shift

steam_root="$HOME/.steam/root"
proton_path="$steam_root/steamapps/common/Proton - Experimental"
runtime_path="$steam_root/steamapps/common/SteamLinuxRuntime_sniper"
prefix_root="$HOME/.proton_prefixes"
prefix_dir="$prefix_root/$exe_name/pfx"

mkdir -p "$prefix_dir"

export STEAM_COMPAT_DATA_PATH="$prefix_dir/.."
export STEAM_COMPAT_CLIENT_INSTALL_PATH="$steam_root"
export MESA_EXTENSION_MAX_YEAR=2003

cd "$exe_dir" || exit 1

"$runtime_path/_v2-entry-point" --verb=run -- "$proton_path/proton" run "$exe_name" "$@"