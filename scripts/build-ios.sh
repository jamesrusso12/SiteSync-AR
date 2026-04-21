#!/usr/bin/env bash
#
# build-ios.sh
#
# One-shot iOS build + codesign + optional device install for SiteSync AR.
# Wraps UnrealBuildTool so the known Mac-side friction points are handled
# automatically on every build:
#
#   1. Re-applies the Apple_SDK.json MaxVersion patch (in case an Epic
#      Launcher update rolled it back — UE 5.6.1 caps at Xcode 16.9 but
#      this Mac runs Xcode 26).
#   2. Strips com.apple.FinderInfo and com.apple.fileprovider.fpfs#P
#      xattrs from the engine's iOS build resources, so codesign does not
#      fail with "resource fork, Finder information, or similar detritus
#      not allowed". macOS auto-adds these to files copied from iCloud or
#      Launcher-extracted directories and `xattr -cr` alone does not
#      remove them — must use explicit `xattr -d`.
#   3. Runs UBT for SiteSyncAR IOS Development.
#   4. If the first UBT pass fails at the codesign step with the xattr
#      error, strips xattrs from the produced .app and re-runs just the
#      codesign step with the exact args UBT used, then signs the
#      .app successfully.
#   5. (Optional) With `--install`, pushes the signed .app to the
#      currently-connected iPhone via `xcrun devicectl`.
#
# Usage:
#   bash scripts/build-ios.sh            # build + codesign
#   bash scripts/build-ios.sh --install  # also install on connected iPhone
#
# See CLAUDE.md "Xcode 26 / UE 5.6 toolchain patch" for the SDK.json
# patch context, and memory/decisions.md for Team ID / signing notes.

set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
UPROJECT="$PROJECT_ROOT/SiteSyncAR/SiteSyncAR.uproject"
UBT="/Users/Shared/Epic Games/UE_5.6/Engine/Build/BatchFiles/RunUBT.sh"
APP_PATH="$PROJECT_ROOT/SiteSyncAR/Binaries/IOS/SiteSyncAR.app"
XCENT_PATH="$PROJECT_ROOT/SiteSyncAR/Binaries/SiteSyncAR (IOS).build/IOS/SiteSyncAR.build/SiteSyncAR.app.xcent"
ENGINE_IOS_RES="/Users/Shared/Epic Games/UE_5.6/Engine/Build/IOS/Resources"

# Signing identity SHA1 — extracted once from `security find-identity -p codesigning -v`.
# If you rotate certs or switch machines, this will need re-extracting.
SIGN_IDENTITY_SHA1="FF26227A0F297FD9D6E7E7A3097AA6A0EB580FC9"

INSTALL_AFTER=0
for arg in "$@"; do
  case "$arg" in
    --install) INSTALL_AFTER=1 ;;
  esac
done

echo "[1/4] Ensuring UE 5.6 Apple_SDK.json MaxVersion patch is applied..."
bash "$PROJECT_ROOT/scripts/patch-ue56-xcode26.sh"

echo "[2/4] Stripping FinderInfo xattrs from engine iOS resources (preventive)..."
find "$ENGINE_IOS_RES" \( -type f -o -type d \) -exec xattr -d com.apple.FinderInfo {} \; 2>/dev/null || true
find "$ENGINE_IOS_RES" \( -type f -o -type d \) -exec xattr -d 'com.apple.fileprovider.fpfs#P' {} \; 2>/dev/null || true

echo "[3/4] Running UBT: SiteSyncAR IOS Development..."
set +e
"$UBT" SiteSyncAR IOS Development -Project="$UPROJECT" 2>&1 | tee /tmp/sitesync-ubt.log
UBT_EXIT=${PIPESTATUS[0]}
set -e

if [ $UBT_EXIT -ne 0 ]; then
  if grep -q "resource fork, Finder information, or similar detritus not allowed" /tmp/sitesync-ubt.log; then
    echo ""
    echo "  UBT hit the known FinderInfo/codesign issue. Stripping and re-signing manually..."
    find "$APP_PATH" \( -type f -o -type d \) -exec xattr -d com.apple.FinderInfo {} \; 2>/dev/null || true
    find "$APP_PATH" \( -type f -o -type d \) -exec xattr -d 'com.apple.fileprovider.fpfs#P' {} \; 2>/dev/null || true
    /usr/bin/codesign --force --sign "$SIGN_IDENTITY_SHA1" \
      --entitlements "$XCENT_PATH" \
      --timestamp=none --generate-entitlement-der \
      "$APP_PATH"
    echo "  re-codesign ok"
  else
    echo "UBT failed for a reason other than the xattr issue. See /tmp/sitesync-ubt.log."
    exit $UBT_EXIT
  fi
fi

echo "[4/4] Verifying signed .app..."
/usr/bin/codesign --verify --verbose=2 "$APP_PATH"

if [ $INSTALL_AFTER -eq 1 ]; then
  echo ""
  echo "=== Installing on connected iPhone ==="
  DEVICE_ID=$(xcrun devicectl list devices 2>/dev/null | awk '/iPhone/ && /connected/ {print $(NF-2); exit}')
  if [ -z "$DEVICE_ID" ]; then
    echo "error: no connected iPhone found. Plug it in and retry with --install."
    exit 3
  fi
  xcrun devicectl device install app --device "$DEVICE_ID" "$APP_PATH"
  echo ""
  echo "Installed. If this is the first install from this cert, trust the developer:"
  echo "  iPhone → Settings → General → VPN & Device Management → Apple Development → Trust"
fi

echo ""
echo "Build complete: $APP_PATH"
