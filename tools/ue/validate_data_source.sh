#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [ "$#" -lt 2 ]; then
  echo "사용법: $0 <schema.json> <source.csv|json> [extra args...]" >&2
  exit 2
fi

SCHEMA_INPUT="$1"
SOURCE_INPUT="$2"
shift 2

SCHEMA_PATH="$(cd "$(dirname "${SCHEMA_INPUT}")" && pwd)/$(basename "${SCHEMA_INPUT}")"
SOURCE_PATH="$(cd "$(dirname "${SOURCE_INPUT}")" && pwd)/$(basename "${SOURCE_INPUT}")"

exec "${SCRIPT_DIR}/run_commandlet.sh" ValidateDataSource -Schema="${SCHEMA_PATH}" -Source="${SOURCE_PATH}" "$@"
