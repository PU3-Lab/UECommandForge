#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
SAMPLE_DIR="${REPO_ROOT}/sample"

"${REPO_ROOT}/tools/ue/validate_asset_rules.sh" \
  "${REPO_ROOT}/specs/policies/assets.policy.json" \
  -RootPaths=/Game/Tests
REPORT="$(ls -t "${SAMPLE_DIR}/Saved/CodexReports/ValidateAssetRules_"*.json | head -1)"

test -f "${REPORT}"
jq -e '.ok == true' "${REPORT}" >/dev/null
jq -e '.commandlet == "ValidateAssetRules"' "${REPORT}" >/dev/null
jq -e '.validation.asset_count == "11"' "${REPORT}" >/dev/null
jq -e '.validation.issue_count == "0"' "${REPORT}" >/dev/null
