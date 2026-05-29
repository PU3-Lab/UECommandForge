#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
SAMPLE_DIR="${REPO_ROOT}/sample"
UE_TOOLS="${REPO_ROOT}/tools/ue"
FIXTURE_DIR="${SAMPLE_DIR}/Saved/CodexReports/data_import_rollback_smoke"
SMOKE_ID="$(date -u +%Y%m%dT%H%M%SZ)_$$"
TARGET_PACKAGE="/Game/Tests/DataImportSmoke/DT_SmokeImport_${SMOKE_ID}"
TARGET_FILE="${SAMPLE_DIR}/Content/Tests/DataImportSmoke/DT_SmokeImport_${SMOKE_ID}.uasset"

export UE_COMMANDLET_TIMEOUT="${UE_COMMANDLET_TIMEOUT:-90}"

mkdir -p "${FIXTURE_DIR}"

SCHEMA_PATH="${FIXTURE_DIR}/import_schema_${SMOKE_ID}.json"
SOURCE_PATH="${FIXTURE_DIR}/import_source_${SMOKE_ID}.csv"

latest_report() {
  local commandlet="$1"
  ls -t "${SAMPLE_DIR}/Saved/CodexReports/${commandlet}_"*.json | head -1
}

cat >"${SCHEMA_PATH}" <<JSON
{
  "version": "1",
  "kind": "data_schema",
  "source_type": "csv",
  "target_asset_path": "${TARGET_PACKAGE}",
  "row_struct_path": "/Script/GameplayTags.GameplayTagTableRow",
  "fields": [
    { "name": "Tag", "type": "string", "required": true },
    { "name": "DevComment", "type": "string" }
  ]
}
JSON

cat >"${SOURCE_PATH}" <<'CSV'
,Tag,DevComment
0,Codex.Smoke.Row0,first row
1,Codex.Smoke.Row1,second row
CSV

"${UE_TOOLS}/import_data_source.sh" "${SCHEMA_PATH}" "${SOURCE_PATH}" >/dev/null
IMPORT_REPORT="$(latest_report ImportDataSource)"
ROLLBACK_PLAN="$(jq -r '.rollback_plan_path' "${IMPORT_REPORT}")"

jq -e '.ok == true' "${IMPORT_REPORT}" >/dev/null
jq -e '.applied == true' "${IMPORT_REPORT}" >/dev/null
jq -e '.rollback_available == true' "${IMPORT_REPORT}" >/dev/null
jq -e '.post_validation.datatable_asset == "exists"' "${IMPORT_REPORT}" >/dev/null
test -f "${TARGET_FILE}"
test -f "${ROLLBACK_PLAN}"

"${UE_TOOLS}/validate_datatable.sh" "${SCHEMA_PATH}" "${TARGET_PACKAGE}" >/dev/null
VALIDATE_REPORT="$(latest_report ValidateDataTable)"
jq -e '.ok == true' "${VALIDATE_REPORT}" >/dev/null
jq -e '.validation.row_count == "2"' "${VALIDATE_REPORT}" >/dev/null

set +e
"${UE_TOOLS}/rollback_asset_changes.sh" "${ROLLBACK_PLAN}" >/dev/null
ROLLBACK_STATUS=$?
set -e
ROLLBACK_REPORT="$(latest_report RollbackAssetChanges)"
test "${ROLLBACK_STATUS}" = "0"
jq -e '.ok == true' "${ROLLBACK_REPORT}" >/dev/null
jq -e '.applied == true' "${ROLLBACK_REPORT}" >/dev/null
jq -e '.validation.applied_rollback_count == "1"' "${ROLLBACK_REPORT}" >/dev/null
test ! -f "${TARGET_FILE}"

echo "data_import_rollback smoke PASS"
