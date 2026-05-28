#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [ $# -lt 1 ]; then
  echo "사용법: $0 <uht-log.txt> [extra args...]" >&2
  exit 2
fi

UHT_LOG_PATH="$1"
shift

if [ -f "${UHT_LOG_PATH}" ]; then
  UHT_LOG_PATH="$(cd "$(dirname "${UHT_LOG_PATH}")" && pwd)/$(basename "${UHT_LOG_PATH}")"
fi

exec "${SCRIPT_DIR}/run_commandlet.sh" AnalyzeUhtLog -Log="${UHT_LOG_PATH}" "$@"
