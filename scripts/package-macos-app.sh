#!/usr/bin/env bash
# Package a relocatable macOS .app + Apple Silicon DMG for distribution.
# Usage: package-macos-app.sh <binary> <data-dir> <out-dir> [arch-label]
set -euo pipefail

BINARY="$(cd "$(dirname "${1}")" && pwd)/$(basename "${1}")"
DATA_DIR="$(cd "${2}" && pwd)"
mkdir -p "${3}"
OUT_DIR="$(cd "${3}" && pwd)"
ARCH_LABEL="${4:-$(uname -m)}"

APP_NAME="Stunt Car Racer for Lovers"
BUNDLE_ID="com.lovers.stuntcarracer"
DMG_NAME="StuntCarRacerForLovers-macOS-${ARCH_LABEL}.dmg"
DMG_PATH="${OUT_DIR}/${DMG_NAME}"

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
STAGE="${OUT_DIR}/stage-${ARCH_LABEL}"
APP="${STAGE}/${APP_NAME}.app"
CONTENTS="${APP}/Contents"
MACOS="${CONTENTS}/MacOS"
RES="${CONTENTS}/Resources"

rm -rf "${STAGE}"
mkdir -p "${MACOS}" "${RES}"

cp "${BINARY}" "${MACOS}/stuntcarracer"
chmod +x "${MACOS}/stuntcarracer"
cp -R "${DATA_DIR}" "${RES}/data"

cat > "${MACOS}/StuntCarRacerForLovers" <<'EOF'
#!/bin/bash
DIR="$(cd "$(dirname "$0")" && pwd)"
cd "${DIR}/../Resources"
exec "${DIR}/stuntcarracer" "$@"
EOF
chmod +x "${MACOS}/StuntCarRacerForLovers"

ICON_SRC="${ROOT}/package/icon256.png"
if [[ -f "${ICON_SRC}" ]] && command -v sips >/dev/null && command -v iconutil >/dev/null; then
  TMP="$(mktemp -d)"
  ICONSET="${TMP}/AppIcon.iconset"
  mkdir -p "${ICONSET}"
  sips -z 16 16 "${ICON_SRC}" --out "${ICONSET}/icon_16x16.png" >/dev/null
  sips -z 32 32 "${ICON_SRC}" --out "${ICONSET}/icon_16x16@2x.png" >/dev/null
  sips -z 32 32 "${ICON_SRC}" --out "${ICONSET}/icon_32x32.png" >/dev/null
  sips -z 64 64 "${ICON_SRC}" --out "${ICONSET}/icon_32x32@2x.png" >/dev/null
  sips -z 128 128 "${ICON_SRC}" --out "${ICONSET}/icon_128x128.png" >/dev/null
  sips -z 256 256 "${ICON_SRC}" --out "${ICONSET}/icon_128x128@2x.png" >/dev/null
  sips -z 256 256 "${ICON_SRC}" --out "${ICONSET}/icon_256x256.png" >/dev/null
  sips -z 512 512 "${ICON_SRC}" --out "${ICONSET}/icon_256x256@2x.png" >/dev/null
  sips -z 512 512 "${ICON_SRC}" --out "${ICONSET}/icon_512x512.png" >/dev/null
  sips -z 1024 1024 "${ICON_SRC}" --out "${ICONSET}/icon_512x512@2x.png" >/dev/null
  if iconutil -c icns "${ICONSET}" -o "${RES}/AppIcon.icns" 2>/dev/null; then
    echo "Embedded AppIcon.icns"
  fi
  rm -rf "${TMP}"
fi

ICON_XML=""
if [[ -f "${RES}/AppIcon.icns" ]]; then
  ICON_XML=$'\t<key>CFBundleIconFile</key>\n\t<string>AppIcon</string>'
fi

cat > "${CONTENTS}/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>CFBundleDevelopmentRegion</key>
	<string>en</string>
	<key>CFBundleExecutable</key>
	<string>StuntCarRacerForLovers</string>
	<key>CFBundleIdentifier</key>
	<string>${BUNDLE_ID}</string>
	<key>CFBundleInfoDictionaryVersion</key>
	<string>6.0</string>
	<key>CFBundleName</key>
	<string>${APP_NAME}</string>
	<key>CFBundleDisplayName</key>
	<string>${APP_NAME}</string>
	<key>CFBundlePackageType</key>
	<string>APPL</string>
	<key>CFBundleShortVersionString</key>
	<string>1.0</string>
	<key>CFBundleVersion</key>
	<string>1</string>
	<key>LSMinimumSystemVersion</key>
	<string>11.0</string>
	<key>NSHighResolutionCapable</key>
	<true/>
${ICON_XML}
</dict>
</plist>
EOF

# Ad-hoc sign so the bundle is a valid signed app after quarantine is cleared.
# Full Gatekeeper pass still needs an Apple Developer ID + notarization ($99/yr).
if command -v codesign >/dev/null; then
  codesign --force --deep --sign - "${APP}"
  echo "Ad-hoc codesigned ${APP}"
fi

cat > "${STAGE}/HOW-TO-OPEN.txt" <<'EOF'
Stunt Car Racer for Lovers — macOS (Apple Silicon)

1. Open this disk image.
2. Drag "Stunt Car Racer for Lovers.app" into Applications.
3. First launch (unsigned build — Apple shows a security warning):

   Option A — easiest
   Double-click "Fix macOS Gatekeeper.command" in this DMG.
   macOS may ask to allow Terminal; confirm, then the game opens.

   Option B — System Settings
   After the warning, open System Settings → Privacy & Security.
   Scroll down and click "Open Anyway", then confirm.

   Option C — Finder
   Right-click the app → Open → Open (once).

   Option D — Terminal
   xattr -cr "/Applications/Stunt Car Racer for Lovers.app"
   open "/Applications/Stunt Car Racer for Lovers.app"

Controls: U Amiga+ physics · I Speed feel · O Enhanced Look · P Pause

Play in browser: https://nmarchand73.github.io/StuntCarRacerForLovers/play/
EOF

# One-click Gatekeeper bypass for unsigned downloads (clears quarantine attribute).
cat > "${STAGE}/Fix macOS Gatekeeper.command" <<'EOF'
#!/bin/bash
set -euo pipefail
APP_NAME="Stunt Car Racer for Lovers.app"
CANDIDATES=(
  "/Applications/${APP_NAME}"
  "$(cd "$(dirname "$0")" && pwd)/${APP_NAME}"
  "${HOME}/Applications/${APP_NAME}"
)
TARGET=""
for candidate in "${CANDIDATES[@]}"; do
  if [[ -d "${candidate}" ]]; then
    TARGET="${candidate}"
    break
  fi
done
if [[ -z "${TARGET}" ]]; then
  osascript -e 'display alert "App not found" message "Drag \"Stunt Car Racer for Lovers.app\" into Applications first, then run this again." as critical'
  exit 1
fi
xattr -cr "${TARGET}"
open "${TARGET}"
EOF
chmod +x "${STAGE}/Fix macOS Gatekeeper.command"

# DMG layout: app + Applications shortcut + readme + Gatekeeper helper
DMG_ROOT="${OUT_DIR}/dmgroot-${ARCH_LABEL}"
rm -rf "${DMG_ROOT}"
mkdir -p "${DMG_ROOT}"
cp -R "${APP}" "${DMG_ROOT}/"
cp "${STAGE}/HOW-TO-OPEN.txt" "${DMG_ROOT}/"
cp "${STAGE}/Fix macOS Gatekeeper.command" "${DMG_ROOT}/"
ln -s /Applications "${DMG_ROOT}/Applications"

rm -f "${DMG_PATH}"
hdiutil create \
  -volname "${APP_NAME}" \
  -srcfolder "${DMG_ROOT}" \
  -ov \
  -format UDZO \
  "${DMG_PATH}"

rm -rf "${DMG_ROOT}"

echo "Packaged ${DMG_PATH}"
ls -lh "${DMG_PATH}"
