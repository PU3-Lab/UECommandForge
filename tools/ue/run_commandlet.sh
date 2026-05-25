#!/usr/bin/env bash
# 사용법: run_commandlet.sh <CommandletName> [extra-args...]
# Result JSON은 항상 sample/Saved/CodexReports/<Commandlet>_<UTC>.json에 기록된다.

set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/ue_env.sh"

if [ $# -lt 1 ]; then
  echo "사용법: $0 <CommandletName> [extra args...]" >&2
  exit 2
fi

COMMANDLET="$1"
shift

REPORT_DIR="${REPO_ROOT}/sample/Saved/CodexReports"
mkdir -p "${REPORT_DIR}"
STAMP="$(date -u +%Y%m%dT%H%M%SZ)"
OUTPUT_JSON="${REPORT_DIR}/${COMMANDLET}_${STAMP}.json"

"${UNREAL_EDITOR_CMD}" "${PROJECT_FILE}" \
  -run="${COMMANDLET}" \
  -Output="${OUTPUT_JSON}" \
  -unattended -nop4 -nosplash -log -stdout -FullStdOutLogOutput \
  "$@"

UE_EXIT=$?

if [ ! -f "${OUTPUT_JSON}" ]; then
  echo "[run_commandlet] Result JSON이 없습니다: ${OUTPUT_JSON}" >&2
  exit 5
fi

echo "${OUTPUT_JSON}"
exit ${UE_EXIT}
