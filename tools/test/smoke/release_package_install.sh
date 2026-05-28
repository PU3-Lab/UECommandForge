#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
WORK_DIR="${REPO_ROOT}/sample/Saved/CodexReports/ReleasePackageInstall"
VERSION="$(jq -r '.VersionName' "${REPO_ROOT}/sample/Plugins/UECommandForge/UECommandForge.uplugin")"
PROJECT_DIR="${WORK_DIR}/Project"
CODEX_HOME="${WORK_DIR}/CodexHome"

rm -rf "${WORK_DIR}"
mkdir -p "${PROJECT_DIR}" "${CODEX_HOME}"
cp "${REPO_ROOT}/sample/UECommandForgeSample.uproject" "${PROJECT_DIR}/UECommandForgeSample.uproject"

PLUGIN_OUT="${WORK_DIR}/PluginPackage"
TOOLS_OUT="${WORK_DIR}/ToolsPackage"
mkdir -p "${PLUGIN_OUT}" "${TOOLS_OUT}"

"${REPO_ROOT}/tools/release/package_plugin.sh" \
  --version "${VERSION}" \
  --channel install-smoke \
  --out-dir "${PLUGIN_OUT}" \
  --skip-build

"${REPO_ROOT}/tools/release/package_tools.sh" \
  --version "${VERSION}" \
  --channel install-smoke \
  --out-dir "${TOOLS_OUT}"

PLUGIN_ZIP="${PLUGIN_OUT}/UECommandForge-${VERSION}-UE5.7-Mac.zip"
TOOLS_ZIP="${TOOLS_OUT}/UECommandForge-${VERSION}-Tools.zip"
PROJECT_FILE="${PROJECT_DIR}/UECommandForgeSample.uproject"

"${REPO_ROOT}/tools/release/install_local.sh" \
  --project "${PROJECT_FILE}" \
  --plugin-package "${PLUGIN_ZIP}" \
  --tools-package "${TOOLS_ZIP}" \
  --codex-home "${CODEX_HOME}" \
  --run-commandlet-check false

test -f "${PROJECT_DIR}/Plugins/UECommandForge/UECommandForge.uplugin"
test -f "${PROJECT_DIR}/Plugins/UECommandForge/Binaries/Mac/UnrealEditor-UECommandForgeRuntime.dylib"
test -f "${CODEX_HOME}/UECommandForge/tools/ue/run_commandlet.sh"
test -f "${CODEX_HOME}/UECommandForge/specs/policies/assets.policy.json"
test -x "${CODEX_HOME}/UECommandForge/tools/ue/run_commandlet.sh"
test -f "${CODEX_HOME}/UECommandForge/uecommandforge.env"
test -f "${CODEX_HOME}/UECommandForge/uecommandforge-installed.json"
test -f "${PROJECT_DIR}/UECommandForge/uecommandforge-project.json"
test -f "${PROJECT_DIR}/Saved/UECommandForge/install.log"

jq -e \
  --arg project_file "${PROJECT_FILE}" \
  --arg codex_home "${CODEX_HOME}" \
  '.project_file == $project_file
   and .codex_home == $codex_home
   and .installed_version == "0.1.0"' \
  "${PROJECT_DIR}/UECommandForge/uecommandforge-project.json" >/dev/null

jq -e \
  --arg project_file "${PROJECT_FILE}" \
  '.project_file == $project_file
   and .version == "0.1.0"
   and (.checksums | type == "object")' \
  "${CODEX_HOME}/UECommandForge/uecommandforge-installed.json" >/dev/null

grep -q "^UECF_PROJECT_FILE=${PROJECT_FILE}$" \
  "${CODEX_HOME}/UECommandForge/uecommandforge.env"

"${REPO_ROOT}/tools/release/uninstall.sh" \
  --project "${PROJECT_FILE}" \
  --codex-home "${CODEX_HOME}" \
  --remove-codex-tools true \
  --remove-specs true

test ! -e "${PROJECT_DIR}/Plugins/UECommandForge"
test ! -e "${CODEX_HOME}/UECommandForge/tools"
test ! -e "${CODEX_HOME}/UECommandForge/specs"
test ! -e "${PROJECT_DIR}/UECommandForge/uecommandforge-project.json"
test -f "${PROJECT_DIR}/Saved/UECommandForge/uninstall.log"
