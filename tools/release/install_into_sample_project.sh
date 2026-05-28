#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
PROJECT="${REPO_ROOT}/sample/UECommandForgeSample.uproject"

exec "${SCRIPT_DIR}/install_local.sh" --project "${PROJECT}" "$@"
