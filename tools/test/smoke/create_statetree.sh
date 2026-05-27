#!/usr/bin/env bash
# StateTree 생성 end-to-end smoke test.
# 사용법: ./tools/test/smoke/create_statetree.sh <spec_file>
#   예시: ./tools/test/smoke/create_statetree.sh specs/examples/guard_ai.json
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

echo "=== StateTree Create Smoke Test ==="
echo "Spec: ${SPEC_FILE}"
echo ""

"${UE_TOOLS}/create_statetree.sh" "${SPEC_FILE}"
REPORT="$(ls -t "${SAMPLE_DIR}/Saved/CodexReports/CreateStateTree_"*.json | head -1)"

OK=$(jq -r '.ok' "${REPORT}")
COMPILE=$(jq -r '.validation.compile_status // "missing"' "${REPORT}")
ON_DISK=$(jq -r '.validation.asset_on_disk // "missing"' "${REPORT}")
IN_REG=$(jq -r '.validation.asset_in_registry // "missing"' "${REPORT}")
ASSET_PATH=$(jq -r '.created_assets[0]' "${REPORT}" | sed 's|/Game/|Content/|')
UASSET="${SAMPLE_DIR}/${ASSET_PATH}.uasset"

check "ok == true" "${OK}"
check "compile_status == ok" "$([ "${COMPILE}" = "ok" ] && echo true || echo false)"
check "asset_on_disk == true" "${ON_DISK}"
check "asset_in_registry == true" "${IN_REG}"
check ".uasset 파일 존재" "$([ -f "${UASSET}" ] && echo true || echo false)"

echo ""
echo "=== 결과: PASS ${PASS} / FAIL ${FAIL} ==="

if [ "${FAIL}" -gt 0 ]; then
    exit 1
fi
