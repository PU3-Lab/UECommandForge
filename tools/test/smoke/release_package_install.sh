#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
WORK_DIR="${REPO_ROOT}/sample/Saved/CodexReports/ReleasePackageInstall"
VERSION="$(jq -r '.VersionName' "${REPO_ROOT}/sample/Plugins/UECommandForge/UECommandForge.uplugin")"
PROJECT_DIR="${WORK_DIR}/Project"
CODEX_HOME="${WORK_DIR}/CodexHome"
PLATFORM="Mac"
RUNTIME_BINARY="Binaries/Mac/UnrealEditor-UECommandForgeRuntime.dylib"

case "$(uname -s)" in
  Darwin)
    PLATFORM="Mac"
    RUNTIME_BINARY="Binaries/Mac/UnrealEditor-UECommandForgeRuntime.dylib"
    ;;
  Linux)
    PLATFORM="Linux"
    RUNTIME_BINARY="Binaries/Linux/UnrealEditor-UECommandForgeRuntime.so"
    ;;
  MINGW*|MSYS*|CYGWIN*)
    PLATFORM="Win64"
    RUNTIME_BINARY="Binaries/Win64/UnrealEditor-UECommandForgeRuntime.dll"
    ;;
esac

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

PLUGIN_ZIP="${PLUGIN_OUT}/UECommandForge-${VERSION}-UE5.7-${PLATFORM}.zip"
TOOLS_ZIP="${TOOLS_OUT}/UECommandForge-${VERSION}-Tools.zip"
PROJECT_FILE="${PROJECT_DIR}/UECommandForgeSample.uproject"

"${REPO_ROOT}/tools/release/install_local.sh" \
  --project "${PROJECT_FILE}" \
  --plugin-package "${PLUGIN_ZIP}" \
  --tools-package "${TOOLS_ZIP}" \
  --codex-home "${CODEX_HOME}" \
  --run-commandlet-check false

test -f "${PROJECT_DIR}/Plugins/UECommandForge/UECommandForge.uplugin"
test -f "${PROJECT_DIR}/Plugins/UECommandForge/${RUNTIME_BINARY}"
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
test ! -e "${CODEX_HOME}/UECommandForge/uecommandforge.env"
test ! -e "${CODEX_HOME}/UECommandForge/uecommandforge-installed.json"
test ! -e "${PROJECT_DIR}/UECommandForge/uecommandforge-project.json"
test -f "${PROJECT_DIR}/Saved/UECommandForge/uninstall.log"

UNMANAGED_PROJECT="${WORK_DIR}/UnmanagedProject"
UNMANAGED_CODEX="${WORK_DIR}/UnmanagedCodex"
mkdir -p "${UNMANAGED_PROJECT}/Plugins/UECommandForge" "${UNMANAGED_CODEX}"
cp "${REPO_ROOT}/sample/UECommandForgeSample.uproject" "${UNMANAGED_PROJECT}/UECommandForgeSample.uproject"
if "${REPO_ROOT}/tools/release/install_local.sh" \
  --project "${UNMANAGED_PROJECT}/UECommandForgeSample.uproject" \
  --plugin-package "${PLUGIN_ZIP}" \
  --tools-package "${TOOLS_ZIP}" \
  --codex-home "${UNMANAGED_CODEX}" \
  --run-commandlet-check false >/dev/null 2>&1; then
  echo "unmanaged existing plugin should fail" >&2
  exit 1
fi

SYMLINK_PROJECT="${WORK_DIR}/SymlinkProject"
SYMLINK_TARGET="${WORK_DIR}/SymlinkTarget"
mkdir -p "${SYMLINK_PROJECT}" "${SYMLINK_TARGET}"
cp "${REPO_ROOT}/sample/UECommandForgeSample.uproject" "${SYMLINK_PROJECT}/UECommandForgeSample.uproject"
ln -s "${SYMLINK_TARGET}" "${SYMLINK_PROJECT}/Plugins"
if "${REPO_ROOT}/tools/release/install_local.sh" \
  --project "${SYMLINK_PROJECT}/UECommandForgeSample.uproject" \
  --plugin-package "${PLUGIN_ZIP}" \
  --tools-package "${TOOLS_ZIP}" \
  --codex-home "${WORK_DIR}/SymlinkCodex" \
  --run-commandlet-check false >/dev/null 2>&1; then
  echo "symlinked Plugins directory should fail" >&2
  exit 1
fi

MISSING_CHECKSUM_DIR="${WORK_DIR}/MissingChecksum"
mkdir -p "${MISSING_CHECKSUM_DIR}"
cp "${PLUGIN_ZIP}" "${MISSING_CHECKSUM_DIR}/$(basename "${PLUGIN_ZIP}")"
if "${REPO_ROOT}/tools/release/install_local.sh" \
  --project "${PROJECT_FILE}" \
  --plugin-package "${MISSING_CHECKSUM_DIR}/$(basename "${PLUGIN_ZIP}")" \
  --tools-package "${TOOLS_ZIP}" \
  --codex-home "${WORK_DIR}/MissingChecksumCodex" \
  --run-commandlet-check false >/dev/null 2>&1; then
  echo "missing external checksum should fail" >&2
  exit 1
fi
