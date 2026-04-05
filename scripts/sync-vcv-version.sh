#!/usr/bin/env bash
# Sync VCV Rack plugin.json version with VERSION file

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
VERSION_FILE="$PROJECT_DIR/VERSION"
PLUGIN_JSON="$PROJECT_DIR/plugins/octobir/vcv-rack/plugin.json"

# Read version from VERSION file
if [ ! -f "$VERSION_FILE" ]; then
  echo "Error: VERSION file not found at $VERSION_FILE"
  exit 1
fi

VERSION=$(cat "$VERSION_FILE" | tr -d '[:space:]')
if [ -z "$VERSION" ]; then
  echo "Error: VERSION file is empty"
  exit 1
fi

echo "Syncing VCV plugin version to: $VERSION"

# Update plugin.json version field using sed (macOS and Linux compatible)
if [[ "$OSTYPE" == "darwin"* ]]; then
  # macOS requires empty string after -i
  sed -i '' "s/\"version\": \"[^\"]*\"/\"version\": \"$VERSION\"/" "$PLUGIN_JSON"
else
  # Linux
  sed -i "s/\"version\": \"[^\"]*\"/\"version\": \"$VERSION\"/" "$PLUGIN_JSON"
fi

echo "✓ Updated plugin.json version to $VERSION"

# Verify the change
CURRENT_VERSION=$(grep -o '"version": "[^"]*"' "$PLUGIN_JSON" | cut -d'"' -f4)
if [ "$CURRENT_VERSION" = "$VERSION" ]; then
  echo "✓ Verification passed: plugin.json version is $CURRENT_VERSION"
else
  echo "✗ Verification failed: Expected $VERSION but got $CURRENT_VERSION"
  exit 1
fi
