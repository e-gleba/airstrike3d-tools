#!/bin/bash
# Grab clipboard image on Linux and save as PNG.
# Wayland (wl-paste) -> X11 (xclip) -> error.
# Usage: grab_clipboard.sh <output.png>
#
# Pattern from telegramdesktop/tdesktop@dev, .claude/grab_clipboard.sh
# (https://github.com/telegramdesktop/tdesktop/blob/dev/.claude/grab_clipboard.sh),
# which is macOS-only (osascript). This Linux implementation is original.
# Same license as this repo (MIT, see license.md).
set -u
outPath="$1"
if [ -z "${outPath:-}" ]; then
    echo "Usage: grab_clipboard.sh <output.png>"
    exit 1
fi
if command -v wl-paste >/dev/null 2>&1; then
    if wl-paste --list-types 2>/dev/null | grep -qi image; then
        wl-paste --type image/png > "$outPath" && echo "Saved to $outPath" && exit 0
    fi
fi
if command -v xclip >/dev/null 2>&1; then
    if xclip -selection clipboard -t image/png -o > "$outPath" 2>/dev/null; then
        echo "Saved to $outPath" && exit 0
    fi
fi
echo "No image on clipboard (need wl-paste or xclip)"
exit 1
