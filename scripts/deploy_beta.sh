#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

python3 "${PROJECT_DIR}/scripts/build_web_bundle.py" --channel beta
exec python3 "${PROJECT_DIR}/scripts/deploy_update_server.py" --channel beta "$@"
