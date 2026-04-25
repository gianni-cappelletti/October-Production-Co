#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VST3_DIR="${HOME}/.vst3"

INSTALL_OCTOBASS=ask
INSTALL_OCTOBIR=ask

usage() {
    cat <<EOF
October Production Co. Plugins - Linux installer

Usage: $0 [OPTIONS]

  --all         Install all plugins without prompting
  --octobass    Install OctoBASS without prompting
  --octobir     Install OctobIR without prompting
  -h, --help    Show this help and exit

With no flags, the installer prompts for each plugin interactively.
Plugins are installed to: $VST3_DIR
EOF
}

for arg in "$@"; do
    case "$arg" in
        --all)        INSTALL_OCTOBASS=yes; INSTALL_OCTOBIR=yes ;;
        --octobass)   INSTALL_OCTOBASS=yes ;;
        --octobir)    INSTALL_OCTOBIR=yes ;;
        -h|--help)    usage; exit 0 ;;
        *)            echo "Unknown option: $arg" >&2; usage >&2; exit 1 ;;
    esac
done

prompt_for() {
    local var_name="$1" plugin_label="$2"
    if [ "$(eval "echo \$$var_name")" = "ask" ]; then
        read -r -p "Install $plugin_label? [Y/n] " answer
        case "${answer:-y}" in
            [Yy]*) eval "$var_name=yes" ;;
            *)     eval "$var_name=no" ;;
        esac
    fi
}

echo "October Production Co. Plugins Installer"
echo "========================================"
echo

prompt_for INSTALL_OCTOBASS OctoBASS
prompt_for INSTALL_OCTOBIR  OctobIR

if [ "$INSTALL_OCTOBASS" != "yes" ] && [ "$INSTALL_OCTOBIR" != "yes" ]; then
    echo "No plugins selected. Nothing to do."
    exit 0
fi

mkdir -p "$VST3_DIR"

if [ "$INSTALL_OCTOBASS" = "yes" ]; then
    if [ -d "$SCRIPT_DIR/OctoBASS.vst3" ]; then
        echo "Installing OctoBASS to $VST3_DIR"
        cp -r "$SCRIPT_DIR/OctoBASS.vst3" "$VST3_DIR/"
    else
        echo "Warning: OctoBASS.vst3 not found alongside install.sh, skipping"
    fi
fi

if [ "$INSTALL_OCTOBIR" = "yes" ]; then
    if [ -d "$SCRIPT_DIR/OctobIR.vst3" ]; then
        echo "Installing OctobIR to $VST3_DIR"
        cp -r "$SCRIPT_DIR/OctobIR.vst3" "$VST3_DIR/"
    else
        echo "Warning: OctobIR.vst3 not found alongside install.sh, skipping"
    fi
fi

echo
echo "Installation complete!"
