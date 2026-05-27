#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/../../ue/ue_env.sh"

REPORT_DIR="${REPO_ROOT}/sample/Saved/AutomationReports"
mkdir -p "${REPORT_DIR}"

TIMEOUT_SEC="${UE_TEST_TIMEOUT:-300}"

if command -v timeout &>/dev/null; then
  TIMEOUT_CMD="timeout"
elif command -v gtimeout &>/dev/null; then
  TIMEOUT_CMD="gtimeout"
else
  TIMEOUT_CMD=""
fi

if [ -n "${TIMEOUT_CMD}" ]; then
  echo "[test] 자동화 테스트 시작 (timeout: ${TIMEOUT_SEC}s)"
  "${TIMEOUT_CMD}" "${TIMEOUT_SEC}" \
    "${UNREAL_EDITOR_CMD}" "${PROJECT_FILE}" \
      -unattended -nop4 -nosplash -nullrhi \
      -ExecCmds="Automation RunTests UECommandForge; Quit" \
      -ReportExportPath="${REPORT_DIR}" \
      -log -stdout -FullStdOutLogOutput \
    || {
      EC=$?
      if [ "${EC}" -eq 124 ]; then
        echo "[test] FAIL: ${TIMEOUT_SEC}초 초과로 강제 종료" >&2
      fi
      exit "${EC}"
    }
else
  echo "[test] 자동화 테스트 시작 (timeout 없음 — gtimeout 설치 권장: brew install coreutils)"
  "${UNREAL_EDITOR_CMD}" "${PROJECT_FILE}" \
    -unattended -nop4 -nosplash -nullrhi \
    -ExecCmds="Automation RunTests UECommandForge; Quit" \
    -ReportExportPath="${REPORT_DIR}" \
    -log -stdout -FullStdOutLogOutput
fi

echo ""
echo "=== 자동화 테스트 결과 ==="

# UE가 생성하는 index.json 파싱 (없으면 파일 목록만 출력)
INDEX="${REPORT_DIR}/index.json"
if [ -f "${INDEX}" ]; then
  SUCCEEDED=$(jq -r '.succeeded // .Succeeded // "?"' "${INDEX}")
  FAILED=$(jq -r '.failed    // .Failed    // "?"' "${INDEX}")
  NOT_RUN=$(jq -r '.notRun   // .NotRun    // "?"' "${INDEX}")
  echo "  PASS: ${SUCCEEDED}"
  echo "  FAIL: ${FAILED}"
  echo "  SKIP: ${NOT_RUN}"

  # 실패 테스트 이름 출력
  FAILED_NAMES=$(jq -r '
    (.tests // .Tests // [])[]
    | select((.state // .State) == "Fail")
    | "    - " + (.testDisplayName // .fullTestPath // .name // "unknown")
  ' "${INDEX}" 2>/dev/null || true)
  if [ -n "${FAILED_NAMES}" ]; then
    echo "  실패 항목:"
    echo "${FAILED_NAMES}"
  fi

  if [ "${FAILED}" != "0" ] && [ "${FAILED}" != "?" ]; then
    exit 1
  fi
else
  echo "  리포트 파일 없음: ${INDEX}"
  echo "  생성된 파일:"
  ls "${REPORT_DIR}" 2>/dev/null || echo "  (없음)"
  exit 1
fi
