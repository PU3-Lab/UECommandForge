#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
OUT_DIR="${REPO_ROOT}/sample/Saved/CodexReports/WindowsReleaseValidation"
REPORT="${OUT_DIR}/windows-package-wrapper-validation.json"

rm -rf "${OUT_DIR}"
mkdir -p "${OUT_DIR}"

"${REPO_ROOT}/tools/release/write_windows_validation_report.sh" \
  --output "${REPORT}"

jq -e '
  .status == "blocked"
  and .reason == "windows_host_required"
  and (.static_checks[] | select(. == "tools/test/smoke/windows_command_wrappers.sh"))
  and (.required_windows_commands[] | select(. == "tools\\release\\package_plugin.bat"))
  and (.required_windows_commands[] | select(. == "tools\\release\\install_local.bat"))
' "${REPORT}" >/dev/null
