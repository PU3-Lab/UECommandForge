#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
SAMPLE_DIR="${REPO_ROOT}/sample"
SPEC_PATH="${REPO_ROOT}/specs/examples/cpp_health_component.json"

export UE_COMMANDLET_TIMEOUT="${UE_COMMANDLET_TIMEOUT:-90}"

rm -f "${SAMPLE_DIR}/Saved/CodexReports/GenerateCppClass_"*.json

"${REPO_ROOT}/tools/ue/generate_cpp_class.sh" "${SPEC_PATH}" >/dev/null
REPORT="$(ls -t "${SAMPLE_DIR}/Saved/CodexReports/GenerateCppClass_"*.json | head -1)"

test -f "${REPORT}"
jq -e '.ok == true' "${REPORT}" >/dev/null
jq -e '.commandlet == "GenerateCppClass"' "${REPORT}" >/dev/null
jq -e '.dry_run == true' "${REPORT}" >/dev/null
jq -e '.applied == false' "${REPORT}" >/dev/null
jq -e '.validation.class_symbol == "UCFHealthComponent"' "${REPORT}" >/dev/null
jq -e '.validation.header_preview | contains("UCLASS(BlueprintType")' "${REPORT}" >/dev/null
jq -e '.validation.header_sha1 | length == 40' "${REPORT}" >/dev/null
jq -e '(.changed_files | length) == 0' "${REPORT}" >/dev/null
