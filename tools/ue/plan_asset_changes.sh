#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [ $# -lt 1 ]; then
  echo "사용법: $0 <plan.json> [extra args...]" >&2
  exit 2
fi

PLAN_PATH="$1"
shift

exec "${SCRIPT_DIR}/run_commandlet.sh" PlanAssetChanges -Plan="${PLAN_PATH}" "$@"
