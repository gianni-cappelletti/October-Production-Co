#!/bin/bash
set -euo pipefail

REPO_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
OUTPUT_DIR="${REPO_DIR}/build/mod-sim"
PLAYWRIGHT_IMAGE=mcr.microsoft.com/playwright/python:v1.48.0-jammy

mkdir -p "${OUTPUT_DIR}"
docker run --rm --network container:opc-mod-sim \
    -v "${REPO_DIR}/scripts/mod-sim:/scripts:ro" \
    -v "${OUTPUT_DIR}:/out" \
    "${PLAYWRIGHT_IMAGE}" \
    bash -c "pip install -q playwright==1.48.0 >/dev/null 2>&1 && python3 /scripts/ui_check.py"
