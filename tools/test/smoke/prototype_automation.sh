#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
SAMPLE_DIR="${REPO_ROOT}/sample"
UE_TOOLS="${REPO_ROOT}/tools/ue"
FIXTURE_DIR="${SAMPLE_DIR}/Saved/UECommandForge/Reports/prototype_automation_smoke"
REPORT_DIR="${FIXTURE_DIR}/reports"

export UE_COMMANDLET_TIMEOUT="${UE_COMMANDLET_TIMEOUT:-120}"
export UECF_REPORT_DIR="${REPORT_DIR}"

mkdir -p "${FIXTURE_DIR}" "${REPORT_DIR}"

ASSET_POLICY="${FIXTURE_DIR}/asset_policy.json"
BUILDCS_POLICY="${FIXTURE_DIR}/buildcs_policy.json"
DATA_SCHEMA="${FIXTURE_DIR}/data_schema.json"
DATA_SOURCE="${FIXTURE_DIR}/data.csv"
CONFIG_RULES="${FIXTURE_DIR}/config_rules.json"
CONFIG_INI="${FIXTURE_DIR}/config.ini"

cat >"${ASSET_POLICY}" <<'JSON'
{
  "version": "1",
  "kind": "asset_policy",
  "rules": [
    { "asset_class": "*", "allowed_path": "/Game/Tests" }
  ]
}
JSON

cat >"${BUILDCS_POLICY}" <<'JSON'
{
  "version": "1",
  "kind": "buildcs_policy",
  "modules": [
    {
      "name": "UECommandForgeRuntime",
      "required_public": ["Core"],
      "required_private": []
    }
  ]
}
JSON

cat >"${DATA_SCHEMA}" <<'JSON'
{
  "version": "1",
  "kind": "data_schema",
  "key_field": "id",
  "source_type": "csv",
  "fields": [
    { "name": "id", "type": "string", "required": true, "unique": true },
    { "name": "level", "type": "int", "required": true, "min": 1, "max": 10 }
  ]
}
JSON

cat >"${DATA_SOURCE}" <<'CSV'
id,level
row_1,5
row_2,7
CSV

cat >"${CONFIG_RULES}" <<'JSON'
{
  "version": "1",
  "kind": "config_rules",
  "config_section": "/Script/UECommandForgeSmoke.ProjectRules",
  "fields": [
    { "name": "bEnabled", "type": "bool", "required": true },
    { "name": "MaxCount", "type": "int", "required": true, "min": 1, "max": 10 }
  ]
}
JSON

cat >"${CONFIG_INI}" <<'INI'
[/Script/UECommandForgeSmoke.ProjectRules]
bEnabled=true
MaxCount=5
INI

RUN_LOG="${FIXTURE_DIR}/prototype_automation_run.log"

"${UE_TOOLS}/validate_project_rules.sh" \
  -AssetPolicy="${ASSET_POLICY}" \
  -AssetRootPaths="/Game/Tests" \
  -BuildCsPolicy="${BUILDCS_POLICY}" \
  -DataSchema="${DATA_SCHEMA}" \
  -DataSource="${DATA_SOURCE}" \
  -ConfigRules="${CONFIG_RULES}" \
  -Config="${CONFIG_INI}" >"${RUN_LOG}"

REPORT="$(tail -n 1 "${RUN_LOG}")"
test -f "${REPORT}"
MARKDOWN="$(jq -r '.validation.markdown_path' "${REPORT}")"

jq -e '.ok == true' "${REPORT}" >/dev/null
jq -e '.validation.suite_count == "4"' "${REPORT}" >/dev/null
jq -e '.validation.failed_suite_count == "0"' "${REPORT}" >/dev/null
jq -e '.steps | length == 4' "${REPORT}" >/dev/null
test -f "${MARKDOWN}"
grep -q 'PrototypeAutomation Report' "${MARKDOWN}"
grep -q 'ValidateConfigRules' "${MARKDOWN}"

echo "prototype_automation smoke PASS"
