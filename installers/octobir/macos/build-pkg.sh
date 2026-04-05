#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../../.." && pwd)"
BUILD_DIR="$PROJECT_DIR/build/release/plugins/octobir/juce/OctobIR_artefacts/Release"
DIST_DIR="$PROJECT_DIR/dist"

# Read version from VERSION file
VERSION=$(cat "$PROJECT_DIR/VERSION" | tr -d '[:space:]')
if [ -z "$VERSION" ]; then
  echo "Error: Could not read version from VERSION file"
  exit 1
fi
echo "Building version: $VERSION"

SIGNING_IDENTITY="${SIGNING_IDENTITY:--}"

if [ "$SIGNING_IDENTITY" = "-" ]; then
    SIGN_ARGS=""
else
    SIGN_ARGS="--sign $SIGNING_IDENTITY"
fi

echo "Building macOS installer packages..."

mkdir -p "$DIST_DIR/pkgs"

if [ -d "$BUILD_DIR/AU/OctobIR.component" ]; then
    echo "Building AU package..."
    pkgbuild --root "$BUILD_DIR/AU" \
        --identifier com.octobir.octobir.au \
        --version "$VERSION" \
        --scripts "$SCRIPT_DIR/scripts" \
        --install-location "$HOME/Library/Audio/Plug-Ins/Components" \
        $SIGN_ARGS \
        "$DIST_DIR/pkgs/OctobIR-AU.pkg"
else
    echo "Warning: AU component not found, skipping AU package"
fi

if [ -d "$BUILD_DIR/VST3/OctobIR.vst3" ]; then
    echo "Building VST3 package..."
    pkgbuild --root "$BUILD_DIR/VST3" \
        --identifier com.octobir.octobir.vst3 \
        --version "$VERSION" \
        --install-location "$HOME/Library/Audio/Plug-Ins/VST3" \
        $SIGN_ARGS \
        "$DIST_DIR/pkgs/OctobIR-VST3.pkg"
else
    echo "Warning: VST3 plugin not found, skipping VST3 package"
fi

echo "Building distribution package..."
productbuild --distribution "$SCRIPT_DIR/distribution.xml" \
    --package-path "$DIST_DIR/pkgs" \
    --resources "$SCRIPT_DIR" \
    $SIGN_ARGS \
    "$DIST_DIR/OctobIR-$VERSION-macOS.pkg"

echo "Creating DMG..."
hdiutil create -volname "OctobIR $VERSION" \
    -srcfolder "$DIST_DIR/OctobIR-$VERSION-macOS.pkg" \
    -ov -format UDZO \
    "$DIST_DIR/OctobIR-$VERSION-macOS.dmg"

echo "✓ macOS installer created: $DIST_DIR/OctobIR-$VERSION-macOS.dmg"
