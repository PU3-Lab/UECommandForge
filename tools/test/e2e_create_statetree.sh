#!/usr/bin/env bash
# End-to-end smoke test — CreateStateTree 커맨드렛 검증
# 사용법: ./tools/test/e2e_create_statetree.sh [spec_file]
#   예시: ./tools/test/e2e_create_statetree.sh specs/examples/guard_statetree.json
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SAMPLE_DIR="$(cd "${SCRIPT_DIR}/../../sample" && pwd)"
UE_TOOLS="${SCRIPT_DIR}/../ue"
SPEC_FILE="${1:-${SCRIPT_DIR}/../../specs/examples/guard_statetree.json}"

PASS=0
FAIL=0

check() {
    local desc="$1"
    local result="$2"
    if [ "$result" = "true" ]; then
        echo "  PASS: ${desc}"
        PASS=$((PASS + 1))
    else
        echo "  FAIL: ${desc}"
        FAIL=$((FAIL + 1))
    fi
}

echo "=== Phase 4 Smoke Test — StateTree 생성 ==="
echo "Spec: ${SPEC_FILE}"
echo ""

echo "[1] CreateStateTree"
"${UE_TOOLS}/create_statetree.sh" "${SPEC_FILE}"

REPORT="$(ls -t "${SAMPLE_DIR}/Saved/CodexReports/CreateStateTree_"*.json 2>/dev/null | head -1)"
if [ -z "${REPORT}" ]; then
    echo "  FAIL: 리포트 파일이 생성되지 않았습니다"
    exit 1
fi

ST_OK=$(jq -r '.ok' "${REPORT}")
ST_COMMANDLET=$(jq -r '.commandlet // "missing"' "${REPORT}")
ST_ASSET_PATH=$(jq -r '.created_assets[0] // ""' "${REPORT}" | sed 's|/Game/|Content/|')
ST_UASSET="${SAMPLE_DIR}/${ST_ASSET_PATH}.uasset"
ST_ERROR_COUNT=$(jq -r '.errors | length' "${REPORT}")

check "ok == true" "${ST_OK}"
check "commandlet == CreateStateTree" "$([ "${ST_COMMANDLET}" = "CreateStateTree" ] && echo true || echo false)"
check "created_assets 에 경로 기록됨" "$([ -n "${ST_ASSET_PATH}" ] && echo true || echo false)"
check ".uasset 파일 디스크에 존재" "$([ -f "${ST_UASSET}" ] && echo true || echo false)"
check "errors 없음" "$([ "${ST_ERROR_COUNT}" = "0" ] && echo true || echo false)"

echo ""
echo "리포트: ${REPORT}"
if [ "${FAIL}" -gt 0 ]; then
    echo "에러 상세:"
    jq '.errors' "${REPORT}" 2>/dev/null || true
fi
echo ""
echo "=== 결과: PASS ${PASS} / FAIL ${FAIL} ==="

if [ "${FAIL}" -gt 0 ]; then
    exit 1
fi
