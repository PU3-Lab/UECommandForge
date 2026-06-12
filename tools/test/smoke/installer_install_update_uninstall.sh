#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
WORK_DIR="${REPO_ROOT}/sample/Saved/UECommandForge/Reports/InstallerInstallUpdateUninstall"
VERSION="$(jq -r '.VersionName' "${REPO_ROOT}/sample/Plugins/UECommandForge/UECommandForge.uplugin")"
PROJECT_DIR="${WORK_DIR}/Project"
# HOME 격리 레이아웃
export HOME="${WORK_DIR}"
CODEX_HOME="${WORK_DIR}/.codex"
PLATFORM="Mac"

case "$(uname -s)" in
  Darwin) PLATFORM="Mac" ;;
  Linux) PLATFORM="Linux" ;;
  MINGW*|MSYS*|CYGWIN*) PLATFORM="Win64" ;;
esac

rm -rf "${WORK_DIR}"
mkdir -p "${PROJECT_DIR}" "${CODEX_HOME}"
cp "${REPO_ROOT}/sample/UECommandForgeSample.uproject" "${PROJECT_DIR}/UECommandForgeSample.uproject"

PLUGIN_OUT="${WORK_DIR}/PluginPackage"
TOOLS_OUT="${WORK_DIR}/ToolsPackage"
mkdir -p "${PLUGIN_OUT}" "${TOOLS_OUT}"

"${REPO_ROOT}/tools/release/package_plugin.sh" \
  --version "${VERSION}" \
  --channel installer-smoke \
  --out-dir "${PLUGIN_OUT}" \
  --skip-build

"${REPO_ROOT}/tools/release/package_tools.sh" \
  --version "${VERSION}" \
  --channel installer-smoke \
  --out-dir "${TOOLS_OUT}"

PLUGIN_ZIP="${PLUGIN_OUT}/UECommandForge-${VERSION}-UE5.7-${PLATFORM}.zip"
TOOLS_ZIP="${TOOLS_OUT}/UECommandForge-${VERSION}-Tools.zip"
PROJECT_FILE="${PROJECT_DIR}/UECommandForgeSample.uproject"

# 설치 실행 (--codex 플래그 사용)
"${REPO_ROOT}/tools/release/install_local.sh" \
  --project "${PROJECT_FILE}" \
  --plugin-package "${PLUGIN_ZIP}" \
  --tools-package "${TOOLS_ZIP}" \
  --codex \
  --run-commandlet-check false

printf 'local edit\n' > "${PROJECT_DIR}/Plugins/UECommandForge/local-edit.txt"
printf 'local tool edit\n' > "${CODEX_HOME}/UECommandForge/tools/local-tool-edit.txt"
printf 'local spec edit\n' > "${CODEX_HOME}/UECommandForge/specs/local-spec-edit.txt"

# 업데이트 실행 (--codex 플래그 사용)
"${REPO_ROOT}/tools/release/update_install.sh" \
  --project "${PROJECT_FILE}" \
  --plugin-package "${PLUGIN_ZIP}" \
  --tools-package "${TOOLS_ZIP}" \
  --codex \
  --run-commandlet-check false

test -f "${PROJECT_DIR}/Plugins/UECommandForge/UECommandForge.uplugin"
test ! -f "${PROJECT_DIR}/Plugins/UECommandForge/local-edit.txt"
test ! -f "${CODEX_HOME}/UECommandForge/tools/local-tool-edit.txt"
test ! -f "${CODEX_HOME}/UECommandForge/specs/local-spec-edit.txt"

find "${PROJECT_DIR}/Saved/UECommandForge/Backups" \
  -path '*/Plugins/UECommandForge/local-edit.txt' \
  -type f | grep -q .
find "${PROJECT_DIR}/Saved/UECommandForge/Backups" \
  -path '*/codex/tools/local-tool-edit.txt' \
  -type f | grep -q .
find "${PROJECT_DIR}/Saved/UECommandForge/Backups" \
  -path '*/codex/specs/local-spec-edit.txt' \
  -type f | grep -q .

printf 'forced backup\n' > "${PROJECT_DIR}/Plugins/UECommandForge/forced-backup.txt"
"${REPO_ROOT}/tools/release/update_install.sh" \
  --project "${PROJECT_FILE}" \
  --plugin-package "${PLUGIN_ZIP}" \
  --tools-package "${TOOLS_ZIP}" \
  --codex \
  --run-commandlet-check false \
  --backup false
test ! -f "${PROJECT_DIR}/Plugins/UECommandForge/forced-backup.txt"
find "${PROJECT_DIR}/Saved/UECommandForge/Backups" \
  -path '*/Plugins/UECommandForge/forced-backup.txt' \
  -type f | grep -q .

# 언인스톨 실행 (--codex 플래그 사용)
"${REPO_ROOT}/tools/release/uninstall.sh" \
  --project "${PROJECT_FILE}" \
  --codex \
  --remove-agent-tools true \
  --remove-specs true

test ! -e "${PROJECT_DIR}/Plugins/UECommandForge"
test ! -e "${CODEX_HOME}/UECommandForge/tools"
test ! -e "${CODEX_HOME}/UECommandForge/specs"

echo "INSTALLER UPDATE/UNINSTALL SMOKE TEST PASSED!"
