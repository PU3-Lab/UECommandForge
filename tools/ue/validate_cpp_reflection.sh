#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [ $# -lt 1 ]; then
  echo "사용법: $0 <cpp-reflection-policy.json> [extra args...]" >&2
  exit 2
fi

POLICY_PATH="$1"
shift

if [ -f "${POLICY_PATH}" ]; then
  POLICY_PATH="$(cd "$(dirname "${POLICY_PATH}")" && pwd)/$(basename "${POLICY_PATH}")"
fi

exec "${SCRIPT_DIR}/run_commandlet.sh" ValidateCppReflection -Policy="${POLICY_PATH}" "$@"
