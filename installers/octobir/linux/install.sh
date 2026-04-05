#!/usr/bin/env bash
set -e

echo "OctobIR Audio Plugin Installer"
echo "=============================="
echo ""

VST3_DIR="${HOME}/.vst3"

mkdir -p "$VST3_DIR"

if [ -d "OctobIR.vst3" ]; then
    echo "Installing VST3 plugin to $VST3_DIR"
    cp -r OctobIR.vst3 "$VST3_DIR/"
    echo "✓ VST3 installed"
else
    echo "⚠ VST3 plugin not found"
fi

echo ""
echo "Installation complete!"
