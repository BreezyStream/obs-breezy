#!/bin/bash
# Builds, signs, packages, notarizes and staples the macOS plugin installer.
# Output: release/obs-breezy-<version>-macos-arm64.pkg

set -euo pipefail
cd "$(dirname "$0")"

APP_IDENTITY="Developer ID Application: Jean-Philippe Dery (2QFKX2YRS9)"
PKG_IDENTITY="Developer ID Installer: Jean-Philippe Dery (2QFKX2YRS9)"

VERSION=$(python3 -c "import json; print(json.load(open('buildspec.json'))['version'])")
OUTPUT="release/obs-breezy-${VERSION}-macos-arm64.pkg"

echo "==> Releasing obs-breezy ${VERSION}"

. ~/.config/breezy/notarize.env

echo "==> Building"
./build-mac-release.sh

echo "==> Signing plugin"
codesign --force --deep --options runtime --timestamp \
  --sign "$APP_IDENTITY" \
  build_macos/Release/obs-breezy.plugin
codesign --verify --strict --verbose=2 build_macos/Release/obs-breezy.plugin

echo "==> Packaging"
rm -rf release
cmake --install build_macos --config Release --prefix release

echo "==> Signing package"
productsign --sign "$PKG_IDENTITY" release/obs-breezy.pkg "$OUTPUT"
rm release/obs-breezy.pkg

echo "==> Notarizing"
xcrun notarytool submit "$OUTPUT" \
  --key "$APPLE_API_KEY" \
  --key-id "$APPLE_API_KEY_ID" \
  --issuer "$APPLE_API_ISSUER" \
  --wait

echo "==> Stapling"
xcrun stapler staple "$OUTPUT"
pkgutil --check-signature "$OUTPUT"

echo "==> Done: $OUTPUT"
