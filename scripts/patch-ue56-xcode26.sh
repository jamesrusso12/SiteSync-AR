#!/usr/bin/env bash
#
# patch-ue56-xcode26.sh
#
# Re-applies the Apple_SDK.json MaxVersion bump required to let UE 5.6.1's
# UnrealBuildTool accept Xcode 26.x as a valid Apple toolchain.
#
# Epic's stock Apple_SDK.json ships with MaxVersion "16.9.0", which predates
# Xcode 26's release. UBT then registers IOS/Mac as "buildable: False" and
# any iOS build fails with:
#   "Platform IOS is not a valid platform to build."
#
# UE 5.6's source (StringView.h et al) is already clang-19-clean, so bumping
# the gate is safe. This script is idempotent — re-run it after any Epic
# Launcher update to UE 5.6.x that may reset the file.
#
# Usage: bash scripts/patch-ue56-xcode26.sh
#
# See CLAUDE.md "Xcode 26 / UE 5.6 toolchain patch" section for context.

set -euo pipefail

SDK_JSON="/Users/Shared/Epic Games/UE_5.6/Engine/Config/Apple/Apple_SDK.json"
TARGET_MAX="27.0.0"
ORIGINAL_MAX="16.9.0"

if [[ ! -f "$SDK_JSON" ]]; then
  echo "error: $SDK_JSON not found — is UE 5.6 installed at /Users/Shared/Epic Games/UE_5.6?" >&2
  exit 1
fi

if grep -q "\"MaxVersion\": \"$TARGET_MAX\"" "$SDK_JSON"; then
  echo "ok: Apple_SDK.json already patched (MaxVersion=$TARGET_MAX)"
  exit 0
fi

if ! grep -q "\"MaxVersion\": \"$ORIGINAL_MAX\"" "$SDK_JSON"; then
  echo "warn: Apple_SDK.json MaxVersion line does not match expected '$ORIGINAL_MAX'."
  echo "      Inspect manually before patching:"
  grep "MaxVersion" "$SDK_JSON" || true
  exit 2
fi

cp "$SDK_JSON" "${SDK_JSON}.bak.$(date +%Y%m%d-%H%M%S)"
sed -i '' "s/\"MaxVersion\": \"$ORIGINAL_MAX\"/\"MaxVersion\": \"$TARGET_MAX\"/" "$SDK_JSON"

echo "patched: $SDK_JSON"
echo "         MaxVersion $ORIGINAL_MAX → $TARGET_MAX"
echo "         backup saved next to original (*.bak.*)"
