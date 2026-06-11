#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
SAMPLE_DIR="${REPO_ROOT}/sample"
WORK_DIR="${SAMPLE_DIR}/Saved/UECommandForge/Reports/UhtLogSmoke"
LOG_PATH="${WORK_DIR}/uht_errors.log"
MARKDOWN_PATH="${WORK_DIR}/uht_errors.md"

export UE_COMMANDLET_TIMEOUT="${UE_COMMANDLET_TIMEOUT:-90}"

mkdir -p "${WORK_DIR}"
rm -f "${SAMPLE_DIR}/Saved/UECommandForge/Reports/AnalyzeUhtLog_"*.json
rm -f "${MARKDOWN_PATH}"

cat > "${LOG_PATH}" <<'LOG'
LogInit: Running UnrealHeaderTool
/Project/Plugins/UECommandForge/Source/CodexBad/Public/BadComponent.h(17): Error: Unrecognized type 'FInventoryRow' - type must be a UCLASS, USTRUCT or UENUM
/Project/Plugins/UECommandForge/Source/CodexBad/Public/BadComponent.h(31): Error: BlueprintReadWrite should not be used on private members
/Project/Plugins/UECommandForge/Source/CodexBad/Public/BadComponent.h(42): Warning: Property has no Category specified
LOG

set +e
"${REPO_ROOT}/tools/ue/analyze_uht_log.sh" "${LOG_PATH}" -Markdown="${MARKDOWN_PATH}" >/dev/null
EXIT_CODE=$?
set -e

test "${EXIT_CODE}" -eq 4
REPORT="$(ls -t "${SAMPLE_DIR}/Saved/UECommandForge/Reports/AnalyzeUhtLog_"*.json | head -1)"

test -f "${REPORT}"
test -f "${MARKDOWN_PATH}"
jq -e '.ok == false' "${REPORT}" >/dev/null
jq -e '.commandlet == "AnalyzeUhtLog"' "${REPORT}" >/dev/null
jq -e '.validation.error_count == "2"' "${REPORT}" >/dev/null
jq -e '.validation.warning_count == "1"' "${REPORT}" >/dev/null
jq -e '.issues[] | select(.code == "UHT_UNKNOWN_REFLECTED_TYPE")' "${REPORT}" >/dev/null
jq -e '.issues[] | select(.code == "UHT_PRIVATE_BLUEPRINT_ACCESS")' "${REPORT}" >/dev/null
jq -e '.issues[] | select(.code == "UHT_MISSING_CATEGORY")' "${REPORT}" >/dev/null
grep -q 'AnalyzeUhtLog Report' "${MARKDOWN_PATH}"
grep -q 'UCLASS/USTRUCT/UENUM' "${MARKDOWN_PATH}"
