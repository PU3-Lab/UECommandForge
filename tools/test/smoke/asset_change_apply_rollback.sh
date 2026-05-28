#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
SAMPLE_DIR="${REPO_ROOT}/sample"
PLAN_PATH="${REPO_ROOT}/specs/policies/asset_changes.plan.json"

export UE_COMMANDLET_TIMEOUT="${UE_COMMANDLET_TIMEOUT:-90}"

latest_report() {
  local commandlet="$1"
  ls -t "${SAMPLE_DIR}/Saved/CodexReports/${commandlet}_"*.json | head -1
}

folder_disk_path() {
  local long_package_path="$1"
  local relative_path="${long_package_path#/Game/}"
  printf '%s/Content/%s\n' "${SAMPLE_DIR}" "${relative_path}"
}

cleanup_report_artifacts() {
  rm -f "${SAMPLE_DIR}/Saved/CodexReports/PlanAssetChanges_"*.json
  rm -f "${SAMPLE_DIR}/Saved/CodexReports/ApplyAssetChanges_"*.json
  rm -f "${SAMPLE_DIR}/Saved/CodexReports/RollbackAssetChanges_"*.json
  rm -f "${SAMPLE_DIR}/Saved/CodexReports/rollback_tx-sample-asset-change-plan.json"
  rm -f "${SAMPLE_DIR}/Saved/CodexReports/snapshot_tx-sample-asset-change-plan.json"
}

cleanup_report_artifacts

"${REPO_ROOT}/tools/ue/plan_asset_changes.sh" "${PLAN_PATH}" >/dev/null
PLAN_REPORT="$(latest_report PlanAssetChanges)"
ROLLBACK_PLAN="$(jq -r '.rollback_plan_path' "${PLAN_REPORT}")"

test -f "${PLAN_REPORT}"
test -f "${ROLLBACK_PLAN}"
jq -e '.ok == true' "${PLAN_REPORT}" >/dev/null

"${REPO_ROOT}/tools/ue/apply_asset_changes.sh" "${PLAN_PATH}" >/dev/null
APPLY_REPORT="$(latest_report ApplyAssetChanges)"

test -f "${APPLY_REPORT}"
jq -e '.ok == true' "${APPLY_REPORT}" >/dev/null
jq -e '.applied == true' "${APPLY_REPORT}" >/dev/null
jq -e '.rollback_available == true' "${APPLY_REPORT}" >/dev/null
jq -e '.validation.applied_change_count == "3"' "${APPLY_REPORT}" >/dev/null
test -d "$(folder_disk_path /Game/Tests/Planned)"

"${REPO_ROOT}/tools/ue/rollback_asset_changes.sh" "${ROLLBACK_PLAN}" >/dev/null
ROLLBACK_REPORT="$(latest_report RollbackAssetChanges)"

test -f "${ROLLBACK_REPORT}"
jq -e '.ok == true' "${ROLLBACK_REPORT}" >/dev/null
jq -e '.applied == true' "${ROLLBACK_REPORT}" >/dev/null
jq -e '.validation.applied_rollback_count == "5"' "${ROLLBACK_REPORT}" >/dev/null
test ! -d "$(folder_disk_path /Game/Tests/Planned)"
