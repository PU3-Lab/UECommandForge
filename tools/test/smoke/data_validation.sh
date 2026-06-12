#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
SAMPLE_DIR="${REPO_ROOT}/sample"
UE_TOOLS="${REPO_ROOT}/tools/ue"
FIXTURE_DIR="${SAMPLE_DIR}/Saved/UECommandForge/Reports/data_validation_smoke"

export UE_COMMANDLET_TIMEOUT="${UE_COMMANDLET_TIMEOUT:-90}"

mkdir -p "${FIXTURE_DIR}"

write_data_schema() {
  local path="$1"
  cat >"${path}" <<'JSON'
{
  "version": "1",
  "kind": "data_schema",
  "key_field": "id",
  "source_type": "csv",
  "fields": [
    { "name": "id", "type": "string", "required": true, "unique": true },
    { "name": "level", "type": "int", "required": true, "min": 1, "max": 10 },
    { "name": "rarity", "type": "string", "required": true, "allowed_values": ["common", "rare"] }
  ]
}
JSON
}

run_validation() {
  local schema="$1"
  local source="$2"
  set +e
  "${UE_TOOLS}/validate_data_source.sh" "${schema}" "${source}" >/dev/null
  local status=$?
  set -e
  local report
  report="$(ls -t "${SAMPLE_DIR}/Saved/UECommandForge/Reports/ValidateDataSource_"*.json | head -1)"
  printf '%s\n%s\n' "${status}" "${report}"
}

require_issue() {
  local report="$1"
  local code="$2"
  jq -e --arg code "${code}" 'any(.issues[]?; .code == $code)' "${report}" >/dev/null
}

DATA_SCHEMA="${FIXTURE_DIR}/data_schema.json"
write_data_schema "${DATA_SCHEMA}"

cat >"${FIXTURE_DIR}/ok.csv" <<'CSV'
id,level,rarity
row_1,5,common
row_2,7,rare
CSV

cat >"${FIXTURE_DIR}/duplicate_id.csv" <<'CSV'
id,level,rarity
row_1,5,common
row_1,7,rare
CSV

cat >"${FIXTURE_DIR}/missing_required.csv" <<'CSV'
id,rarity
row_1,common
CSV

cat >"${FIXTURE_DIR}/range_violation.csv" <<'CSV'
id,level,rarity
row_1,42,common
CSV

RESULT_PATH="${FIXTURE_DIR}/last_result.txt"

run_validation "${DATA_SCHEMA}" "${FIXTURE_DIR}/ok.csv" >"${RESULT_PATH}"
STATUS="$(sed -n '1p' "${RESULT_PATH}")"
REPORT="$(sed -n '2p' "${RESULT_PATH}")"
test "${STATUS}" = "0"
jq -e '.ok == true' "${REPORT}" >/dev/null

run_validation "${DATA_SCHEMA}" "${FIXTURE_DIR}/duplicate_id.csv" >"${RESULT_PATH}"
STATUS="$(sed -n '1p' "${RESULT_PATH}")"
REPORT="$(sed -n '2p' "${RESULT_PATH}")"
test "${STATUS}" != "0"
require_issue "${REPORT}" DATA_SOURCE_DUPLICATE_VALUE

run_validation "${DATA_SCHEMA}" "${FIXTURE_DIR}/missing_required.csv" >"${RESULT_PATH}"
STATUS="$(sed -n '1p' "${RESULT_PATH}")"
REPORT="$(sed -n '2p' "${RESULT_PATH}")"
test "${STATUS}" != "0"
require_issue "${REPORT}" DATA_SOURCE_REQUIRED_FIELD_MISSING

run_validation "${DATA_SCHEMA}" "${FIXTURE_DIR}/range_violation.csv" >"${RESULT_PATH}"
STATUS="$(sed -n '1p' "${RESULT_PATH}")"
REPORT="$(sed -n '2p' "${RESULT_PATH}")"
test "${STATUS}" != "0"
require_issue "${REPORT}" DATA_SOURCE_RANGE_VIOLATION

cat >"${FIXTURE_DIR}/config_rules.json" <<'JSON'
{
  "version": "1",
  "kind": "config_rules",
  "config_section": "/Script/UECommandForgeSmoke.Settings",
  "fields": [
    { "name": "bEnabled", "type": "bool", "required": true },
    { "name": "MaxCount", "type": "int", "required": true, "min": 1, "max": 10 }
  ]
}
JSON

cat >"${FIXTURE_DIR}/config.ini" <<'INI'
[/Script/UECommandForgeSmoke.Settings]
bEnabled=true
MaxCount=5
INI

"${UE_TOOLS}/validate_config_rules.sh" "${FIXTURE_DIR}/config_rules.json" "${FIXTURE_DIR}/config.ini" >/dev/null
CONFIG_REPORT="$(ls -t "${SAMPLE_DIR}/Saved/UECommandForge/Reports/ValidateConfigRules_"*.json | head -1)"
jq -e '.ok == true' "${CONFIG_REPORT}" >/dev/null

cat >"${FIXTURE_DIR}/config_invalid.ini" <<'INI'
[/Script/UECommandForgeSmoke.Settings]
bEnabled=true
MaxCount=42
INI

set +e
"${UE_TOOLS}/validate_config_rules.sh" "${FIXTURE_DIR}/config_rules.json" "${FIXTURE_DIR}/config_invalid.ini" >/dev/null
CONFIG_STATUS=$?
set -e
CONFIG_REPORT="$(ls -t "${SAMPLE_DIR}/Saved/UECommandForge/Reports/ValidateConfigRules_"*.json | head -1)"
test "${CONFIG_STATUS}" != "0"
require_issue "${CONFIG_REPORT}" CONFIG_RANGE_VIOLATION

echo "data_validation smoke PASS"
