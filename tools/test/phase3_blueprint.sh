#!/usr/bin/env bash
# Phase 3 end-to-end smoke test
# 사용법: ./tools/test/phase3_blueprint.sh <spec_file>
#   예시: ./tools/test/phase3_blueprint.sh specs/examples/guard_ai.json
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

echo "=== Phase 3 Smoke Test ==="
echo "Spec: ${SPEC_FILE}"
echo ""

# --- Character Blueprint ---
echo "[1] CreateCharacterBlueprint"
"${UE_TOOLS}/create_character_bp.sh" "${SPEC_FILE}"
CHAR_REPORT="$(ls -t "${SAMPLE_DIR}/Saved/CodexReports/CreateCharacterBlueprint_"*.json | head -1)"

CHAR_OK=$(jq -r '.ok' "${CHAR_REPORT}")
CHAR_COMPILE=$(jq -r '.validation.compile_status // "missing"' "${CHAR_REPORT}")
CHAR_ON_DISK=$(jq -r '.validation.asset_on_disk // "missing"' "${CHAR_REPORT}")
CHAR_IN_REG=$(jq -r 'if .validation | has("asset_in_registry") then "true" else "false" end' "${CHAR_REPORT}")
CHAR_ASSET_PATH=$(jq -r '.created_assets[0]' "${CHAR_REPORT}" | sed 's|/Game/|Content/|')
CHAR_UASSET="${SAMPLE_DIR}/${CHAR_ASSET_PATH}.uasset"

check "ok == true" "${CHAR_OK}"
check "compile_status == ok" "$([ "${CHAR_COMPILE}" = "ok" ] && echo true || echo false)"
check "asset_on_disk == true" "${CHAR_ON_DISK}"
check "asset_in_registry 키 존재" "${CHAR_IN_REG}"
check ".uasset 파일 존재" "$([ -f "${CHAR_UASSET}" ] && echo true || echo false)"

echo ""

# --- AIController Blueprint ---
echo "[2] CreateAIControllerBlueprint"
"${UE_TOOLS}/create_ai_controller.sh" "${SPEC_FILE}"
AI_REPORT="$(ls -t "${SAMPLE_DIR}/Saved/CodexReports/CreateAIControllerBlueprint_"*.json | head -1)"

AI_OK=$(jq -r '.ok' "${AI_REPORT}")
AI_COMPILE=$(jq -r '.validation.compile_status // "missing"' "${AI_REPORT}")
AI_ON_DISK=$(jq -r '.validation.asset_on_disk // "missing"' "${AI_REPORT}")
AI_IN_REG=$(jq -r 'if .validation | has("asset_in_registry") then "true" else "false" end' "${AI_REPORT}")
AI_ASSET_PATH=$(jq -r '.created_assets[0]' "${AI_REPORT}" | sed 's|/Game/|Content/|')
AI_UASSET="${SAMPLE_DIR}/${AI_ASSET_PATH}.uasset"

check "ok == true" "${AI_OK}"
check "compile_status == ok" "$([ "${AI_COMPILE}" = "ok" ] && echo true || echo false)"
check "asset_on_disk == true" "${AI_ON_DISK}"
check "asset_in_registry 키 존재" "${AI_IN_REG}"
check ".uasset 파일 존재" "$([ -f "${AI_UASSET}" ] && echo true || echo false)"

echo ""
echo "=== 결과: PASS ${PASS} / FAIL ${FAIL} ==="

if [ "${FAIL}" -gt 0 ]; then
    exit 1
fi
