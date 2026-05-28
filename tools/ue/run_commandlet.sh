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
TIMEOUT_SEC="${UE_COMMANDLET_TIMEOUT:-300}"

case "${TIMEOUT_SEC}" in
  ''|*[!0-9]*)
    echo "[run_commandlet] UE_COMMANDLET_TIMEOUT은 초 단위 정수여야 합니다: ${TIMEOUT_SEC}" >&2
    exit 2
    ;;
esac

run_unreal_commandlet() {
  "${UNREAL_EDITOR_CMD}" "${PROJECT_FILE}" \
    -run="${COMMANDLET}" \
    -Output="${OUTPUT_JSON}" \
    -unattended -nop4 -nosplash -nullrhi -log -stdout -FullStdOutLogOutput \
    "$@"
}

set +e
if [ "${TIMEOUT_SEC}" -eq 0 ]; then
  run_unreal_commandlet "$@"
  UE_EXIT=$?
else
  echo "[run_commandlet] ${COMMANDLET} 시작 (timeout: ${TIMEOUT_SEC}s)" >&2
  run_unreal_commandlet "$@" &
  UE_PID=$!
  ELAPSED=0

  while kill -0 "${UE_PID}" 2>/dev/null; do
    if [ "${ELAPSED}" -ge "${TIMEOUT_SEC}" ]; then
      echo "[run_commandlet] FAIL: ${COMMANDLET}가 ${TIMEOUT_SEC}초를 초과했습니다." >&2
      kill "${UE_PID}" 2>/dev/null || true
      sleep 5
      kill -0 "${UE_PID}" 2>/dev/null && kill -KILL "${UE_PID}" 2>/dev/null
      wait "${UE_PID}" 2>/dev/null
      exit 124
    fi

    sleep 1
    ELAPSED=$((ELAPSED + 1))
  done

  wait "${UE_PID}"
  UE_EXIT=$?
fi
set -e

if [ ! -f "${OUTPUT_JSON}" ]; then
  echo "[run_commandlet] Result JSON이 없습니다: ${OUTPUT_JSON}" >&2
  exit 5
fi

echo "${OUTPUT_JSON}"
exit ${UE_EXIT}
