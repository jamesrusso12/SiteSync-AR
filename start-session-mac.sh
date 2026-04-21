#!/bin/bash
# Pull latest from GitHub (including LFS assets), then open UE5 editor.
# Run this every time you sit down to work on Mac.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
UPROJECT="$SCRIPT_DIR/SiteSyncAR/SiteSyncAR.uproject"
UE5="/Users/Shared/Epic Games/UE_5.5/Engine/Binaries/Mac/UnrealEditor.app"

echo "==> Pulling latest from origin..."
git -C "$SCRIPT_DIR" pull

echo "==> Fetching LFS assets..."
git -C "$SCRIPT_DIR" lfs pull

echo "==> Opening SiteSyncAR in UE5..."
open -a "$UE5" "$UPROJECT"

echo "Done. UE5 is launching."
echo ""
echo "REMINDER — when you finish working:"
echo "  1. Save All in UE5 (Ctrl+Shift+S)"
echo "  2. Run: cd $(dirname $UPROJECT) && git add Content/ Source/ Config/ && git commit -m 'feat: ...' && git push"
