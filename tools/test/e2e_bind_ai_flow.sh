#!/usr/bin/env bash
# End-to-end smoke test — BindAIFlow / ValidateAIFlow 커맨드렛 검증
# 사용법: ./tools/test/e2e_bind_ai_flow.sh [spec_file]
#   예시: ./tools/test/e2e_bind_ai_flow.sh specs/examples/guard_ai.json
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SAMPLE_DIR="$(cd "${SCRIPT_DIR}/../../sample" && pwd)"
UE_TOOLS="${SCRIPT_DIR}/../ue"
SPEC_FILE="${1:-${SCRIPT_DIR}/../../specs/examples/guard_ai.json}"

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

echo "=== E2E Test — AI 플로우 바인딩 ==="
echo "Spec: ${SPEC_FILE}"
echo ""

# --- BindAIFlow ---
echo "[1] BindAIFlow"
"${UE_TOOLS}/bind_ai_flow.sh" "${SPEC_FILE}"
BIND_REPORT="$(ls -t "${SAMPLE_DIR}/Saved/CodexReports/BindAIFlow_"*.json 2>/dev/null | head -1)"
if [ -z "${BIND_REPORT}" ]; then
    echo "  FAIL: BindAIFlow 리포트가 생성되지 않았습니다"
    exit 1
fi

BIND_OK=$(jq -r '.ok' "${BIND_REPORT}")
BIND_COMMANDLET=$(jq -r '.commandlet // "missing"' "${BIND_REPORT}")
BIND_MODIFIED=$(jq -r '.modified_assets | length' "${BIND_REPORT}")
BIND_ERRORS=$(jq -r '.errors | length' "${BIND_REPORT}")

check "ok == true" "${BIND_OK}"
check "commandlet == BindAIFlow" "$([ "${BIND_COMMANDLET}" = "BindAIFlow" ] && echo true || echo false)"
check "modified_assets에 Character 경로 기록됨" "$([ "${BIND_MODIFIED}" -gt 0 ] && echo true || echo false)"
check "errors 없음" "$([ "${BIND_ERRORS}" = "0" ] && echo true || echo false)"

echo ""

# --- ValidateAIFlow ---
echo "[2] ValidateAIFlow"
"${UE_TOOLS}/validate_ai_flow.sh" "${SPEC_FILE}"
VAL_REPORT="$(ls -t "${SAMPLE_DIR}/Saved/CodexReports/ValidateAIFlow_"*.json 2>/dev/null | head -1)"
if [ -z "${VAL_REPORT}" ]; then
    echo "  FAIL: ValidateAIFlow 리포트가 생성되지 않았습니다"
    exit 1
fi

VAL_OK=$(jq -r '.ok' "${VAL_REPORT}")
VAL_COMMANDLET=$(jq -r '.commandlet // "missing"' "${VAL_REPORT}")
VAL_CTRL=$(jq -r '.validation.ai_controller_class // "missing"' "${VAL_REPORT}")
VAL_POSSESS=$(jq -r '.validation.auto_possess_ai // "missing"' "${VAL_REPORT}")
VAL_ERRORS=$(jq -r '.errors | length' "${VAL_REPORT}")

check "ok == true" "${VAL_OK}"
check "commandlet == ValidateAIFlow" "$([ "${VAL_COMMANDLET}" = "ValidateAIFlow" ] && echo true || echo false)"
check "ai_controller_class == ok" "$([ "${VAL_CTRL}" = "ok" ] && echo true || echo false)"
check "auto_possess_ai == ok" "$([ "${VAL_POSSESS}" = "ok" ] && echo true || echo false)"
check "errors 없음" "$([ "${VAL_ERRORS}" = "0" ] && echo true || echo false)"

echo ""
echo "바인딩 리포트: ${BIND_REPORT}"
echo "검증 리포트:   ${VAL_REPORT}"
if [ "${FAIL}" -gt 0 ]; then
    echo "에러 상세 (바인딩):"
    jq '.errors' "${BIND_REPORT}" 2>/dev/null || true
    echo "에러 상세 (검증):"
    jq '.errors' "${VAL_REPORT}" 2>/dev/null || true
fi
echo ""
echo "=== 결과: PASS ${PASS} / FAIL ${FAIL} ==="

if [ "${FAIL}" -gt 0 ]; then
    exit 1
fi
