#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/ue_env.sh"

REPORT_DIR="${REPO_ROOT}/sample/Saved/AutomationReports"
mkdir -p "${REPORT_DIR}"

"${UNREAL_EDITOR_CMD}" "${PROJECT_FILE}" \
  -unattended -nop4 -nosplash -nullrhi \
  -ExecCmds="Automation RunTests UECommandForge; Quit" \
  -ReportOutputPath="${REPORT_DIR}" \
  -log -stdout -FullStdOutLogOutput
