#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/common.sh"

if [ $# -lt 1 ]; then
  echo "Usage: $0 <file> [file...]" >&2
  exit 2
fi

for path in "$@"; do
  if [ ! -f "${path}" ]; then
    echo "[write_checksums] file not found: ${path}" >&2
    exit 2
  fi
  hash="$(uecf_sha256 "${path}")" || exit 2
  printf '%s  %s\n' "${hash}" "$(basename "${path}")"
done
