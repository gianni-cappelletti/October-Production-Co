#!/usr/bin/env bash
# Display ASCII art header with colored logo

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

PLUGIN="${1:-octobir}"
HEADER_FILE="$SCRIPT_DIR/${PLUGIN}_header.txt"

# ANSI color codes
ORANGE='\033[38;5;208m'
RESET='\033[0m'

# Check if header file exists
if [ ! -f "$HEADER_FILE" ]; then
  echo "Warning: Header file not found at $HEADER_FILE"
  exit 0
fi

# Read and print the header file with orange color
while IFS= read -r line; do
  printf "${ORANGE}%s${RESET}\n" "$line"
done < "$HEADER_FILE"
