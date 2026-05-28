#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [ $# -lt 1 ]; then
  echo "사용법: $0 <cpp-class.json> [extra args...]" >&2
  exit 2
fi

SPEC_PATH="$1"
shift

if [ -f "${SPEC_PATH}" ]; then
  SPEC_PATH="$(cd "$(dirname "${SPEC_PATH}")" && pwd)/$(basename "${SPEC_PATH}")"
fi

exec "${SCRIPT_DIR}/run_commandlet.sh" GenerateCppClass -Spec="${SPEC_PATH}" "$@"
