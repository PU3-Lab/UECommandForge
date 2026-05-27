#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
exec "${SCRIPT_DIR}/create_ai_flow.sh" "${REPO_ROOT}/specs/profiles/patrol_ai.json" "$@"
