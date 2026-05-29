#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [ "$#" -lt 1 ]; then
  echo "사용법: $0 <schema.json> [asset_path] [extra args...]" >&2
  exit 2
fi

SCHEMA_INPUT="$1"
shift

SCHEMA_PATH="$(cd "$(dirname "${SCHEMA_INPUT}")" && pwd)/$(basename "${SCHEMA_INPUT}")"
ARGS=(-Schema="${SCHEMA_PATH}")

if [ "$#" -gt 0 ] && [[ "$1" != -* ]]; then
  ARGS+=(-Asset="$1")
  shift
fi

exec "${SCRIPT_DIR}/run_commandlet.sh" ValidateDataTable "${ARGS[@]}" "$@"
