#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [ $# -lt 1 ]; then
  echo "사용법: $0 <diff.allowlist.json> [-Project=...] [-Platforms=...] [-Categories=...] [extra args...]" >&2
  exit 2
fi

ALLOWLIST_PATH="$1"
shift

if [ -f "${ALLOWLIST_PATH}" ]; then
  ALLOWLIST_PATH="$(cd "$(dirname "${ALLOWLIST_PATH}")" && pwd)/$(basename "${ALLOWLIST_PATH}")"
fi

exec "${SCRIPT_DIR}/run_commandlet.sh" DiffPlatformConfig -Allowlist="${ALLOWLIST_PATH}" "$@"
