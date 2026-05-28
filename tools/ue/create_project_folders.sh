#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [ $# -lt 1 ]; then
  echo "사용법: $0 <folders.json> [extra args...]" >&2
  exit 2
fi

SPEC_PATH="$1"
shift

exec "${SCRIPT_DIR}/run_commandlet.sh" CreateProjectFolders -Spec="${SPEC_PATH}" "$@"
