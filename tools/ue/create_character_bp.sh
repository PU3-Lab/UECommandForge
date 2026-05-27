#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [ "$#" -lt 1 ]; then
  echo "사용법: $0 <spec_file> [extra args...]" >&2
  exit 2
fi
SPEC_INPUT="$1"
shift
SPEC_FILE="$(cd "$(dirname "${SPEC_INPUT}")" && pwd)/$(basename "${SPEC_INPUT}")"
exec "${SCRIPT_DIR}/run_commandlet.sh" CreateCharacterBlueprint -SpecFile="${SPEC_FILE}" "$@"
