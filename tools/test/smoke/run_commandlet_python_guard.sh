#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
PROJECT_FILE="${REPO_ROOT}/sample/UECommandForgeSample.uproject"
REPORT_DIR="${REPO_ROOT}/sample/Saved/CodexReports/RunCommandletPythonGuard"

rm -rf "${REPORT_DIR}"
mkdir -p "${REPORT_DIR}"

require_rejected() {
  local pattern="$1"
  shift

  local output
  output="$(mktemp)"
  set +e
  (
    cd "${REPO_ROOT}"
    UNREAL_EDITOR_CMD=/usr/bin/true \
      PROJECT_FILE="${PROJECT_FILE}" \
      UECF_REPORT_DIR="${REPORT_DIR}" \
      ./tools/ue/run_commandlet.sh Hello "$@"
  ) >"${output}" 2>&1
  local status=$?
  set -e

  test "${status}" -eq 2
  grep -q -- "${pattern}" "${output}"
  rm -f "${output}"
}

require_rejected 'Unreal Python access is not allowed' \
  -ExecutePythonScript=/tmp/create_blueprint.py
require_rejected 'Unreal Python access is not allowed' \
  -ExecutePythonCommand='import unreal'
require_rejected 'Unreal Python access is not allowed' \
  -ExecCmds='py import unreal'
require_rejected 'Unreal Python access is not allowed' \
  -ExecCmds='Automation RunTests UECommandForge; py'
require_rejected 'Unreal Python access is not allowed' \
  -ExecCmds='Automation RunTests UECommandForge, py'
require_rejected 'Unreal Python access is not allowed' \
  -ExecCmds=$'Automation RunTests UECommandForge;\tpy print(1)'
require_rejected 'Unreal Python access is not allowed' \
  -ExecCmds $' py print(1)'
require_rejected 'Unreal Python access is not allowed' \
  -ExecCmds='PythonScriptPlugin.ExecutePythonCommand unreal.EditorAssetLibrary'
require_rejected 'Unreal Python access is not allowed' \
  -SomeArg='unreal.AssetToolsHelpers'

output="$(mktemp)"
set +e
(
  cd "${REPO_ROOT}"
  UNREAL_EDITOR_CMD=/usr/bin/true \
    PROJECT_FILE="${PROJECT_FILE}" \
    UECF_REPORT_DIR="${REPORT_DIR}" \
    UE_COMMANDLET_TIMEOUT=0 \
    ./tools/ue/run_commandlet.sh Hello -ExecCmds='Automation RunTests UECommandForge; Quit'
) >"${output}" 2>&1
status=$?
set -e

test "${status}" -eq 5
grep -q -- 'Result JSON' "${output}"
rm -f "${output}"
