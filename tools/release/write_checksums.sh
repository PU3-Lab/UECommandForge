#!/usr/bin/env bash
set -euo pipefail

if [ $# -lt 1 ]; then
  echo "Usage: $0 <file> [file...]" >&2
  exit 2
fi

for path in "$@"; do
  if [ ! -f "${path}" ]; then
    echo "[write_checksums] file not found: ${path}" >&2
    exit 2
  fi
  hash="$(shasum -a 256 "${path}" | awk '{ print $1 }')"
  printf '%s  %s\n' "${hash}" "$(basename "${path}")"
done
