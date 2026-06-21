#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

echo "[build] firmware"
pio run -d "${PROJECT_DIR}"

echo "[build] web package"
python3 "${PROJECT_DIR}/scripts/build_web_bundle.py"

echo "[done] created:"
echo "  ${PROJECT_DIR}/firmware.bin"
echo "  ${PROJECT_DIR}/webui-package.tar"
