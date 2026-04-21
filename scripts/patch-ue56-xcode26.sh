#!/usr/bin/env bash
#
# patch-ue56-xcode26.sh
#
# Re-applies all UE 5.6.1 patches required on this Mac to build iOS under
# Xcode 26.x. Epic's stock UE 5.6.1 predates Xcode 26 and macOS Sequoia+
# xattr behavior, so two surgical engine patches are needed:
#
# PATCH 1: Apple_SDK.json MaxVersion bump (16.9.0 → 27.0.0).
#   Without this, UBT registers iOS/Mac as "buildable: False" and every
#   build fails early with "Platform IOS is not a valid platform to
#   build." UE 5.6's source is already clang-19-clean (modern
#   operator""_PrivateSV spelling), so bumping the version gate is safe.
#
# PATCH 2: XcodeProject.cs xattr strip in the "Copy Executable and Staged
#   Data into .app" build phase. macOS 15+ adds com.apple.FinderInfo and
#   com.apple.fileprovider.fpfs#P xattrs to files copied from iCloud /
#   Launcher-installed dirs, and codesign refuses to sign anything with
#   those attributes ("resource fork, Finder information, or similar
#   detritus not allowed"). Patch injects `xattr -d` calls into the shell
#   phase that runs right before Xcode's built-in codesign step, so
#   every build self-heals.
#
# Run this script idempotently after every Epic Launcher update that
# touches UE 5.6.x. Safe to run every session — detects if patches are
# already applied and skips.
#
# Usage:
#   bash scripts/patch-ue56-xcode26.sh
#
# See CLAUDE.md "Xcode 26 / UE 5.6 toolchain patch" section for context.

set -euo pipefail

ENGINE_ROOT="/Users/Shared/Epic Games/UE_5.6"
SDK_JSON="$ENGINE_ROOT/Engine/Config/Apple/Apple_SDK.json"
XCODE_CS="$ENGINE_ROOT/Engine/Source/Programs/UnrealBuildTool/ProjectFiles/Xcode/XcodeProject.cs"
UBT_DLL="$ENGINE_ROOT/Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.dll"
UBT_CSPROJ="$ENGINE_ROOT/Engine/Source/Programs/UnrealBuildTool/UnrealBuildTool.csproj"
UBT_BUILT_DLL="$ENGINE_ROOT/Engine/Source/Programs/UnrealBuildTool/bin/Development/UnrealBuildTool.dll"
SETUP_ENV="$ENGINE_ROOT/Engine/Build/BatchFiles/Mac/SetupEnvironment.sh"

if [[ ! -d "$ENGINE_ROOT" ]]; then
  echo "error: $ENGINE_ROOT not found — is UE 5.6 installed there?" >&2
  exit 1
fi

REBUILD_UBT=0

# ---- PATCH 1: Apple_SDK.json MaxVersion ----
SDK_TARGET_MAX="27.0.0"
SDK_ORIGINAL_MAX="16.9.0"
if grep -q "\"MaxVersion\": \"$SDK_TARGET_MAX\"" "$SDK_JSON"; then
  echo "ok: Apple_SDK.json already patched (MaxVersion=$SDK_TARGET_MAX)"
elif grep -q "\"MaxVersion\": \"$SDK_ORIGINAL_MAX\"" "$SDK_JSON"; then
  cp "$SDK_JSON" "${SDK_JSON}.bak.$(date +%Y%m%d-%H%M%S)"
  sed -i '' "s/\"MaxVersion\": \"$SDK_ORIGINAL_MAX\"/\"MaxVersion\": \"$SDK_TARGET_MAX\"/" "$SDK_JSON"
  echo "patched: Apple_SDK.json MaxVersion $SDK_ORIGINAL_MAX → $SDK_TARGET_MAX"
else
  echo "warn: Apple_SDK.json MaxVersion does not match expected '$SDK_ORIGINAL_MAX' or target — inspect manually:"
  grep "MaxVersion" "$SDK_JSON" || true
  exit 2
fi

# ---- PATCH 2: XcodeProject.cs xattr strip (separate build phase, unconditional) ----
PATCH2_MARKER="SiteSyncAR patch: strip FinderInfo"
if grep -q "$PATCH2_MARKER" "$XCODE_CS"; then
  echo "ok: XcodeProject.cs already patched (xattr strip phase present)"
else
  cp "$XCODE_CS" "${XCODE_CS}.bak.$(date +%Y%m%d-%H%M%S)"
  python3 - <<'PY'
import pathlib, sys
path = pathlib.Path("/Users/Shared/Epic Games/UE_5.6/Engine/Source/Programs/UnrealBuildTool/ProjectFiles/Xcode/XcodeProject.cs")
text = path.read_text()
anchor = "\t\t\tXcodeShellScriptBuildPhase CopyScriptPhase = new(\"Copy Executable and Staged Data into .app\", CopyScript, Array.Empty<string>(), new string[] { ScriptOutput });\n\t\t\tBuildPhases.Add(CopyScriptPhase);\n\t\t\tReferences.Add(CopyScriptPhase);"
inject = """

\t\t\t// SiteSyncAR patch: strip FinderInfo/fileprovider xattrs that codesign rejects on macOS 15+ / Xcode 26.
\t\t\t// Separate phase (not part of CopyScript) so it runs unconditionally, even when CopyScript exits early
\t\t\t// due to missing staging dir. Must run after CopyScriptPhase, before Xcode's built-in codesign.
\t\t\tList<string> XattrScript = new()
\t\t\t{
\t\t\t\t"set +e",
\t\t\t\t"if [[ -e \\\\\\"${CONFIGURATION_BUILD_DIR}/${CONTENTS_FOLDER_PATH}\\\\\\" ]]; then",
\t\t\t\t"  xattr -dr com.apple.FinderInfo \\\\\\"${CONFIGURATION_BUILD_DIR}/${CONTENTS_FOLDER_PATH}\\\\\\" 2>/dev/null",
\t\t\t\t"  xattr -dr 'com.apple.fileprovider.fpfs#P' \\\\\\"${CONFIGURATION_BUILD_DIR}/${CONTENTS_FOLDER_PATH}\\\\\\" 2>/dev/null",
\t\t\t\t"  xattr -d com.apple.FinderInfo \\\\\\"${CONFIGURATION_BUILD_DIR}/${CONTENTS_FOLDER_PATH}\\\\\\" 2>/dev/null",
\t\t\t\t"  xattr -d 'com.apple.fileprovider.fpfs#P' \\\\\\"${CONFIGURATION_BUILD_DIR}/${CONTENTS_FOLDER_PATH}\\\\\\" 2>/dev/null",
\t\t\t\t"  echo \\\\\\"[SiteSyncAR] stripped FinderInfo xattrs pre-codesign\\\\\\"",
\t\t\t\t"fi",
\t\t\t\t"exit 0",
\t\t\t};
\t\t\tXcodeShellScriptBuildPhase XattrPhase = new(\"Strip FinderInfo xattrs (pre-codesign)\", XattrScript, Array.Empty<string>(), new string[] { \"/dev/null\" });
\t\t\tBuildPhases.Add(XattrPhase);
\t\t\tReferences.Add(XattrPhase);"""
if anchor not in text:
    print("error: anchor not found in XcodeProject.cs — UE may have changed the source; manual patch required", file=sys.stderr)
    sys.exit(3)
new = text.replace(anchor, anchor + inject, 1)
path.write_text(new)
print("patched: XcodeProject.cs xattr-strip phase injected after CopyScriptPhase")
PY
  REBUILD_UBT=1
fi

# ---- Rebuild UBT if we patched its source ----
if [[ $REBUILD_UBT -eq 1 ]]; then
  echo "rebuilding UnrealBuildTool.dll with patched source..."
  bash -c "source \"$SETUP_ENV\" -dotnet \"$ENGINE_ROOT/Engine/Build/BatchFiles/Mac\" && cd \"$ENGINE_ROOT/Engine/Source/Programs/UnrealBuildTool\" && dotnet build UnrealBuildTool.csproj -c Development" >/tmp/ubt-rebuild.log 2>&1
  if [[ ! -f "$UBT_BUILT_DLL" ]]; then
    echo "error: UBT rebuild failed — see /tmp/ubt-rebuild.log" >&2
    exit 4
  fi
  cp "$UBT_BUILT_DLL" "$UBT_DLL"
  echo "installed patched UnrealBuildTool.dll"
fi

echo ""
echo "all patches applied."
