#!/usr/bin/env bash
# AI flow 바인딩 end-to-end smoke test.
# 사용법: ./tools/test/smoke/ai_flow_binding.sh <spec_file>
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SAMPLE_DIR="$(cd "${SCRIPT_DIR}/../../../sample" && pwd)"
UE_TOOLS="${SCRIPT_DIR}/../../ue"
SPEC_FILE="${1:-${SCRIPT_DIR}/../../../specs/examples/guard_ai.json}"

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

echo "=== AI Flow Binding Smoke Test ==="
echo "Spec: ${SPEC_FILE}"
echo ""

"${UE_TOOLS}/create_character_bp.sh" "${SPEC_FILE}" >/dev/null
"${UE_TOOLS}/create_ai_controller.sh" "${SPEC_FILE}" >/dev/null
"${UE_TOOLS}/create_statetree.sh" "${SPEC_FILE}" >/dev/null

"${UE_TOOLS}/bind_ai_flow.sh" "${SPEC_FILE}"
BIND_REPORT="$(ls -t "${SAMPLE_DIR}/Saved/CodexReports/BindAIFlow_"*.json | head -1)"

BIND_OK=$(jq -r '.ok' "${BIND_REPORT}")
BIND_CTRL=$(jq -r '.validation.ai_controller_class // "missing"' "${BIND_REPORT}")
BIND_POSSESS=$(jq -r '.validation.auto_possess_ai // "missing"' "${BIND_REPORT}")
BIND_ST=$(jq -r '.validation.statetree_component // "missing"' "${BIND_REPORT}")

check "bind ok == true" "${BIND_OK}"
check "bind ai_controller_class == ok" "$([ "${BIND_CTRL}" = "ok" ] && echo true || echo false)"
check "bind auto_possess_ai == ok" "$([ "${BIND_POSSESS}" = "ok" ] && echo true || echo false)"
check "bind statetree_component == ok" "$([ "${BIND_ST}" = "ok" ] && echo true || echo false)"

echo ""

"${UE_TOOLS}/validate_ai_flow.sh" "${SPEC_FILE}"
VALIDATE_REPORT="$(ls -t "${SAMPLE_DIR}/Saved/CodexReports/ValidateAIFlow_"*.json | head -1)"

VALIDATE_OK=$(jq -r '.ok' "${VALIDATE_REPORT}")
VALIDATE_CTRL=$(jq -r '.validation.ai_controller_class // "missing"' "${VALIDATE_REPORT}")
VALIDATE_POSSESS=$(jq -r '.validation.auto_possess_ai // "missing"' "${VALIDATE_REPORT}")
VALIDATE_ST=$(jq -r '.validation.statetree_component // "missing"' "${VALIDATE_REPORT}")

check "validate ok == true" "${VALIDATE_OK}"
check "validate ai_controller_class == ok" "$([ "${VALIDATE_CTRL}" = "ok" ] && echo true || echo false)"
check "validate auto_possess_ai == ok" "$([ "${VALIDATE_POSSESS}" = "ok" ] && echo true || echo false)"
check "validate statetree_component == ok" "$([ "${VALIDATE_ST}" = "ok" ] && echo true || echo false)"

echo ""
echo "=== 결과: PASS ${PASS} / FAIL ${FAIL} ==="

if [ "${FAIL}" -gt 0 ]; then
    exit 1
fi
