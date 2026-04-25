#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_ROOT="${BUILD_DIR:-$PROJECT_DIR/build/release}"
DIST_DIR="$PROJECT_DIR/dist"

VERSION="${RELEASE_VERSION:-$(cat "$PROJECT_DIR/VERSION" | tr -d '[:space:]')}"
if [ -z "$VERSION" ]; then
  echo "Error: Could not determine version"
  exit 1
fi
echo "Building version: $VERSION"

SIGNING_IDENTITY="${SIGNING_IDENTITY:--}"
if [ "$SIGNING_IDENTITY" = "-" ]; then
    SIGN_ARGS=""
else
    SIGN_ARGS="--sign $SIGNING_IDENTITY"
fi

mkdir -p "$DIST_DIR/pkgs"

# build_component <plugin-dir> <product-name> <format> <install-location> <pkg-identifier>
# format is VST3 or AU; the on-disk artefact dir is <Product>.vst3 or <Product>.component.
build_component() {
    local plugin="$1" product="$2" format="$3" dest="$4" ident="$5"
    local root="$BUILD_ROOT/plugins/$plugin/juce/${product}_artefacts/Release/$format"
    local artifact_subdir
    if [ "$format" = "AU" ]; then
        artifact_subdir="$product.component"
    else
        artifact_subdir="$product.vst3"
    fi

    if [ -d "$root/$artifact_subdir" ]; then
        echo "Building $product $format package..."
        pkgbuild --root "$root" \
            --identifier "$ident" \
            --version "$VERSION" \
            --scripts "$SCRIPT_DIR/scripts" \
            --install-location "$dest" \
            $SIGN_ARGS \
            "$DIST_DIR/pkgs/${product}-${format}.pkg"
    else
        echo "Warning: $root/$artifact_subdir not found, skipping $product $format"
    fi
}

build_component octobass OctoBASS VST3 "/Library/Audio/Plug-Ins/VST3"       com.octoberprod.octobass.vst3
build_component octobass OctoBASS AU   "/Library/Audio/Plug-Ins/Components" com.octoberprod.octobass.au
build_component octobir  OctobIR  VST3 "/Library/Audio/Plug-Ins/VST3"       com.octoberprod.octobir.vst3
build_component octobir  OctobIR  AU   "/Library/Audio/Plug-Ins/Components" com.octoberprod.octobir.au

# Substitute version into the distribution template
sed "s/{VERSION}/$VERSION/g" "$SCRIPT_DIR/distribution.xml" > "$DIST_DIR/distribution.xml"

echo "Building distribution package..."
productbuild --distribution "$DIST_DIR/distribution.xml" \
    --package-path "$DIST_DIR/pkgs" \
    --resources "$SCRIPT_DIR" \
    $SIGN_ARGS \
    "$DIST_DIR/OctoberPluginsSuite-$VERSION-macOS.pkg"

echo "Creating DMG..."
hdiutil create -volname "October Production Co. Plugins $VERSION" \
    -srcfolder "$DIST_DIR/OctoberPluginsSuite-$VERSION-macOS.pkg" \
    -ov -format UDZO \
    "$DIST_DIR/OctoberPluginsSuite-$VERSION-macOS.dmg"

echo "macOS installer created: $DIST_DIR/OctoberPluginsSuite-$VERSION-macOS.dmg"
