#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

git -C "${PROJECT_DIR}" config core.hooksPath .githooks
chmod +x "${PROJECT_DIR}/.githooks/pre-push"

echo "Git hooks installed. pre-push will now build automatically without deploying."
