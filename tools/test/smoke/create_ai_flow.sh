#!/usr/bin/env bash
# CreateAIFlow end-to-end smoke test.
# 사용법: ./tools/test/smoke/create_ai_flow.sh <spec_file>
#   예시: ./tools/test/smoke/create_ai_flow.sh specs/examples/guard_ai.json
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SAMPLE_DIR="$(cd "${SCRIPT_DIR}/../../../sample" && pwd)"
UE_TOOLS="${SCRIPT_DIR}/../../ue"
SPEC_FILE="${SCRIPT_DIR}/../../../specs/examples/guard_ai.json"

if [ "$#" -gt 0 ]; then
    SPEC_FILE="$1"
    shift
fi

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

echo "=== CreateAIFlow Smoke Test ==="
echo "Spec: ${SPEC_FILE}"
echo ""

export UE_COMMANDLET_TIMEOUT="${UE_COMMANDLET_TIMEOUT:-120}"

"${UE_TOOLS}/create_ai_flow.sh" "${SPEC_FILE}" -nullrhi "$@"
REPORT="$(ls -t "${SAMPLE_DIR}/Saved/UECommandForge/Reports/CreateAIFlow_"*.json | head -1)"

OK=$(jq -r '.ok' "${REPORT}")
ROLLBACK=$(jq -r '.rollback_available' "${REPORT}")
STEP_COUNT=$(jq -r '.steps | length' "${REPORT}")
STEPS_OK=$(jq -r 'all(.steps[]?; .ok == true)' "${REPORT}")
CREATED_COUNT=$(jq -r '.created_assets | length' "${REPORT}")
MODIFIED_COUNT=$(jq -r '.modified_assets | length' "${REPORT}")
COMPILE=$(jq -r '.validation.compile_status // "missing"' "${REPORT}")
ACTOR_PLACED=$(jq -r '.validation.actor_placed // "missing"' "${REPORT}")

check "ok == true" "${OK}"
check "rollback_available == false" "$([ "${ROLLBACK}" = "false" ] && echo true || echo false)"
check "steps length == 5" "$([ "${STEP_COUNT}" = "5" ] && echo true || echo false)"
check "all steps ok == true" "${STEPS_OK}"
check "created_assets length == 3" "$([ "${CREATED_COUNT}" = "3" ] && echo true || echo false)"
check "modified_assets length == 1" "$([ "${MODIFIED_COUNT}" = "1" ] && echo true || echo false)"
check "compile_status == ok" "$([ "${COMPILE}" = "ok" ] && echo true || echo false)"
check "actor_placed == ok" "$([ "${ACTOR_PLACED}" = "ok" ] && echo true || echo false)"

while IFS= read -r asset_path; do
    asset_file="${SAMPLE_DIR}/${asset_path/#\/Game\//Content/}.uasset"
    check "${asset_path} .uasset 파일 존재" "$([ -f "${asset_file}" ] && echo true || echo false)"
done < <(jq -r '.created_assets[]' "${REPORT}")

while IFS= read -r map_path; do
    map_file="${SAMPLE_DIR}/${map_path/#\/Game\//Content/}.umap"
    check "${map_path} .umap 파일 존재" "$([ -f "${map_file}" ] && echo true || echo false)"
done < <(jq -r '.modified_assets[]' "${REPORT}")

echo ""
echo "Report: ${REPORT}"
echo "=== 결과: PASS ${PASS} / FAIL ${FAIL} ==="

if [ "${FAIL}" -gt 0 ]; then
    exit 1
fi
