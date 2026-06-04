#!/usr/bin/env bash
# 범용 Actor Blueprint 생성 end-to-end smoke test
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SAMPLE_DIR="$(cd "${SCRIPT_DIR}/../../../sample" && pwd)"
UE_TOOLS="${SCRIPT_DIR}/../../ue"

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
        echo "  (결과값: ${result})"
        FAIL=$((FAIL + 1))
    fi
}

if ! command -v jq &>/dev/null; then
    echo "에러: 이 스크립트를 구동하려면 'jq' CLI 도구가 설치되어 있어야 합니다." >&2
    exit 1
fi

echo "=== Generic Blueprint Create Smoke Test ==="
echo ""

# 임시 파일 경로
TEMP_SPEC="${SAMPLE_DIR}/Saved/TempSpec.json"
TEMP_OUT="${SAMPLE_DIR}/Saved/TempOut.log"
mkdir -p "$(dirname "${TEMP_SPEC}")"

# 1. 정상 케이스 생성 테스트
echo "[1] Create Standard Actor Blueprint (allow_replace=true)"
cat <<EOF > "${TEMP_SPEC}"
{
  "version": "1",
  "kind": "blueprint",
  "transaction_id": "tx-test-create-actor",
  "asset_path": "/Game/Actors/BP_Door",
  "parent_class": "/Script/Engine.Actor",
  "compile": true,
  "save": true,
  "allow_replace": true
}
EOF

"${UE_TOOLS}/create_blueprint.sh" "${TEMP_SPEC}" > "${TEMP_OUT}"
REPORT_FILE="$(grep -E '\.json$' "${TEMP_OUT}" | tail -1)"
echo "Report: ${REPORT_FILE}"

OK=$(jq -r '.ok' "${REPORT_FILE}")
COMPILE=$(jq -r '.validation.compile_status' "${REPORT_FILE}")
ON_DISK=$(jq -r '.validation.asset_on_disk' "${REPORT_FILE}")
IN_REG=$(jq -r '.validation.asset_in_registry' "${REPORT_FILE}")
ASSET_PATH=$(jq -r '.created_assets[0]' "${REPORT_FILE}" | sed 's|/Game/|Content/|')
UASSET="${SAMPLE_DIR}/${ASSET_PATH}.uasset"

check "ok == true" "${OK}"
check "compile_status == ok" "$([ "${COMPILE}" = "ok" ] && echo true || echo false)"
check "asset_on_disk == true" "${ON_DISK}"
check "asset_in_registry == true" "${IN_REG}"
check ".uasset 파일 존재" "$([ -f "${UASSET}" ] && echo true || echo false)"

echo ""

# 2. 중복 생성 방지 테스트 (allow_replace=false)
echo "[2] Create existing blueprint with allow_replace=false (Should Fail)"
cat <<EOF > "${TEMP_SPEC}"
{
  "version": "1",
  "kind": "blueprint",
  "transaction_id": "tx-test-fail-duplicate",
  "asset_path": "/Game/Actors/BP_Door",
  "parent_class": "/Script/Engine.Actor",
  "compile": true,
  "save": true,
  "allow_replace": false
}
EOF

# 실패 케이스이므로 쉘의 set -e에 의해 스크립트가 죽지 않도록 || true 처리
"${UE_TOOLS}/create_blueprint.sh" "${TEMP_SPEC}" > "${TEMP_OUT}" || true
REPORT_FILE="$(grep -E '\.json$' "${TEMP_OUT}" | tail -1)"

OK=$(jq -r '.ok' "${REPORT_FILE}")
ERR_CODE=$(jq -r '.errors[0].code' "${REPORT_FILE}")

check "ok == false" "$([ "${OK}" = "false" ] && echo true || echo false)"
check "error.code == BP_ALREADY_EXISTS" "$([ "${ERR_CODE}" = "BP_ALREADY_EXISTS" ] && echo true || echo false)"

echo ""

# 3. 부적합 부모 클래스 테스트 (Actor 계층이 아닌 경우)
echo "[3] Create blueprint with non-actor parent class (Should Fail)"
cat <<EOF > "${TEMP_SPEC}"
{
  "version": "1",
  "kind": "blueprint",
  "transaction_id": "tx-test-fail-non-actor",
  "asset_path": "/Game/Actors/BP_Door",
  "parent_class": "/Script/Engine.ActorComponent",
  "compile": true,
  "save": true,
  "allow_replace": true
}
EOF

"${UE_TOOLS}/create_blueprint.sh" "${TEMP_SPEC}" > "${TEMP_OUT}" || true
REPORT_FILE="$(grep -E '\.json$' "${TEMP_OUT}" | tail -1)"

OK=$(jq -r '.ok' "${REPORT_FILE}")
ERR_CODE=$(jq -r '.errors[0].code' "${REPORT_FILE}")

check "ok == false" "$([ "${OK}" = "false" ] && echo true || echo false)"
check "error.code == PARENT_CLASS_NOT_ACTOR" "$([ "${ERR_CODE}" = "PARENT_CLASS_NOT_ACTOR" ] && echo true || echo false)"

echo ""

# 4. 존재하지 않는 부모 클래스 테스트
echo "[4] Create blueprint with invalid parent class (Should Fail)"
cat <<EOF > "${TEMP_SPEC}"
{
  "version": "1",
  "kind": "blueprint",
  "transaction_id": "tx-test-fail-invalid-parent",
  "asset_path": "/Game/Actors/BP_Door",
  "parent_class": "/Script/Engine.NonExistentClass",
  "compile": true,
  "save": true,
  "allow_replace": true
}
EOF

"${UE_TOOLS}/create_blueprint.sh" "${TEMP_SPEC}" > "${TEMP_OUT}" || true
REPORT_FILE="$(grep -E '\.json$' "${TEMP_OUT}" | tail -1)"

OK=$(jq -r '.ok' "${REPORT_FILE}")
ERR_CODE=$(jq -r '.errors[0].code' "${REPORT_FILE}")

check "ok == false" "$([ "${OK}" = "false" ] && echo true || echo false)"
check "error.code == PARENT_CLASS_NOT_FOUND" "$([ "${ERR_CODE}" = "PARENT_CLASS_NOT_FOUND" ] && echo true || echo false)"

echo ""
echo "=== 결과: PASS ${PASS} / FAIL ${FAIL} ==="

rm -f "${TEMP_SPEC}" "${TEMP_OUT}"

if [ "${FAIL}" -gt 0 ]; then
    exit 1
fi

