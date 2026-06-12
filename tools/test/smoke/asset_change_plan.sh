#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
SAMPLE_DIR="${REPO_ROOT}/sample"

"${REPO_ROOT}/tools/ue/plan_asset_changes.sh" \
  "${REPO_ROOT}/specs/policies/asset_changes.plan.json"
REPORT="$(ls -t "${SAMPLE_DIR}/Saved/UECommandForge/Reports/PlanAssetChanges_"*.json | head -1)"
ROLLBACK="$(jq -r '.rollback_plan_path' "${REPORT}")"

test -f "${REPORT}"
test -f "${ROLLBACK}"
jq -e '.ok == true' "${REPORT}" >/dev/null
jq -e '.commandlet == "PlanAssetChanges"' "${REPORT}" >/dev/null
jq -e '.rollback_available == true' "${REPORT}" >/dev/null
jq -e '.validation.operation_count == "3"' "${REPORT}" >/dev/null
jq -e '.validation.planned_change_count == "3"' "${REPORT}" >/dev/null
jq -e '(.operations | length) == 5' "${ROLLBACK}" >/dev/null
