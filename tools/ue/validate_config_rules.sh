#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [ "$#" -lt 1 ]; then
  echo "사용법: $0 <schema.json> [config.ini] [extra args...]" >&2
  exit 2
fi

SCHEMA_INPUT="$1"
shift

SCHEMA_PATH="$(cd "$(dirname "${SCHEMA_INPUT}")" && pwd)/$(basename "${SCHEMA_INPUT}")"
ARGS=(-Schema="${SCHEMA_PATH}")

if [ "$#" -gt 0 ] && [[ "$1" != -* ]]; then
  CONFIG_INPUT="$1"
  CONFIG_PATH="$(cd "$(dirname "${CONFIG_INPUT}")" && pwd)/$(basename "${CONFIG_INPUT}")"
  ARGS+=(-Config="${CONFIG_PATH}")
  shift
fi

exec "${SCRIPT_DIR}/run_commandlet.sh" ValidateConfigRules "${ARGS[@]}" "$@"
