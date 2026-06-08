#!/usr/bin/env bash
# 사용법: ./tools/ue/create_blueprint_batch.sh <spec_file> [extra args...]

set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [ "$#" -lt 1 ]; then
  echo "사용법: $0 <spec_file> [extra args...]" >&2
  exit 2
fi

SPEC_INPUT="$1"
shift

if [ ! -f "${SPEC_INPUT}" ]; then
  echo "에러: 지정한 Spec 파일이 존재하지 않습니다: ${SPEC_INPUT}" >&2
  exit 1
fi

SPEC_FILE="$(cd "$(dirname "${SPEC_INPUT}")" && pwd)/$(basename "${SPEC_INPUT}")"
exec "${SCRIPT_DIR}/run_commandlet.sh" CreateBlueprintBatch -Spec="${SPEC_FILE}" "$@"
