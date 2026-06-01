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
cat > "${CODEX_HOME}/AGENTS.md" <<'AGENTS'
# 개인 Codex 지침

이 내용은 설치 후에도 보존되어야 한다.

<!-- BEGIN UECOMMANDFORGE CODEX INSTRUCTIONS -->
## 오래된 UECommandForge 지침

- OLD_UNREAL_PYTHON_RULE_SHOULD_BE_REPLACED
<!-- END UECOMMANDFORGE CODEX INSTRUCTIONS -->
AGENTS

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
test -f "${CODEX_HOME}/UECommandForge/specs/codex/unreal-automation-agents.md"
test -x "${CODEX_HOME}/UECommandForge/tools/ue/run_commandlet.sh"
test -f "${CODEX_HOME}/UECommandForge/uecommandforge.env"
test -f "${CODEX_HOME}/UECommandForge/uecommandforge-installed.json"
test -f "${PROJECT_DIR}/UECommandForge/uecommandforge-project.json"
test -f "${PROJECT_DIR}/Saved/UECommandForge/install.log"
test -f "${CODEX_HOME}/AGENTS.md"
grep -q '이 내용은 설치 후에도 보존되어야 한다.' "${CODEX_HOME}/AGENTS.md"
grep -q 'BEGIN UECOMMANDFORGE CODEX INSTRUCTIONS' "${CODEX_HOME}/AGENTS.md"
grep -q 'Codex가 하지 말아야 할 일' "${CODEX_HOME}/AGENTS.md"
grep -q '사용자가 요청하더라도 Unreal Python으로 Unreal Editor/프로젝트/에셋에 접근하지 않는다.' "${CODEX_HOME}/AGENTS.md"
grep -q 'ExecCmds.*비 Python 자동화 명령' "${CODEX_HOME}/AGENTS.md"
grep -q '파일을 만들지 않더라도 Python 코드를 Unreal에 전달하거나 실행하지 않는다.' "${CODEX_HOME}/AGENTS.md"
grep -q '코드 조각도 제공하지 말고' "${CODEX_HOME}/AGENTS.md"
grep -q '실패한 commandlet을 우회하기 위해 임시 스크립트' "${CODEX_HOME}/AGENTS.md"
grep -q '기존 commandlet/wrapper 기반 대안' "${CODEX_HOME}/AGENTS.md"
test "$(grep -c 'BEGIN UECOMMANDFORGE CODEX INSTRUCTIONS' "${CODEX_HOME}/AGENTS.md")" -eq 1
! grep -q 'OLD_UNREAL_PYTHON_RULE_SHOULD_BE_REPLACED' "${CODEX_HOME}/AGENTS.md"

jq -e \
  --arg project_file "${PROJECT_FILE}" \
  --arg version "${VERSION}" \
  --arg codex_home "${CODEX_HOME}" \
  '.project_file == $project_file
   and .codex_home == $codex_home
   and .installed_version == $version' \
  "${PROJECT_DIR}/UECommandForge/uecommandforge-project.json" >/dev/null

jq -e \
  --arg project_file "${PROJECT_FILE}" \
  --arg version "${VERSION}" \
  '.project_file == $project_file
   and .version == $version
   and (.checksums | type == "object")' \
  "${CODEX_HOME}/UECommandForge/uecommandforge-installed.json" >/dev/null

grep -q "^UECF_PROJECT_FILE=${PROJECT_FILE}$" \
  "${CODEX_HOME}/UECommandForge/uecommandforge.env"

"${REPO_ROOT}/tools/release/uninstall.sh" \
  --project "${PROJECT_FILE}" \
  --codex-home "${CODEX_HOME}"

test ! -e "${PROJECT_DIR}/Plugins/UECommandForge"
test -e "${CODEX_HOME}/UECommandForge/tools"
test -e "${CODEX_HOME}/UECommandForge/specs"
test -f "${CODEX_HOME}/UECommandForge/uecommandforge.env"
test -f "${CODEX_HOME}/UECommandForge/uecommandforge-installed.json"
test ! -e "${PROJECT_DIR}/UECommandForge/uecommandforge-project.json"

"${REPO_ROOT}/tools/release/install_local.sh" \
  --project "${PROJECT_FILE}" \
  --plugin-package "${PLUGIN_ZIP}" \
  --tools-package "${TOOLS_ZIP}" \
  --codex-home "${CODEX_HOME}" \
  --run-commandlet-check false

test "$(grep -c 'BEGIN UECOMMANDFORGE CODEX INSTRUCTIONS' "${CODEX_HOME}/AGENTS.md")" -eq 1
grep -q '이 내용은 설치 후에도 보존되어야 한다.' "${CODEX_HOME}/AGENTS.md"

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

UNMANAGED_PROJECT_META="${WORK_DIR}/UnmanagedProjectMetadata"
UNMANAGED_PROJECT_META_CODEX="${WORK_DIR}/UnmanagedProjectMetadataCodex"
mkdir -p "${UNMANAGED_PROJECT_META}/UECommandForge" "${UNMANAGED_PROJECT_META_CODEX}"
cp "${REPO_ROOT}/sample/UECommandForgeSample.uproject" \
  "${UNMANAGED_PROJECT_META}/UECommandForgeSample.uproject"
printf '{"unmanaged":true}\n' \
  > "${UNMANAGED_PROJECT_META}/UECommandForge/uecommandforge-project.json"
if "${REPO_ROOT}/tools/release/install_local.sh" \
  --project "${UNMANAGED_PROJECT_META}/UECommandForgeSample.uproject" \
  --plugin-package "${PLUGIN_ZIP}" \
  --tools-package "${TOOLS_ZIP}" \
  --codex-home "${UNMANAGED_PROJECT_META_CODEX}" \
  --run-commandlet-check false >/dev/null 2>&1; then
  echo "unmanaged existing project metadata should fail" >&2
  exit 1
fi

UNMANAGED_CODEX_META_PROJECT="${WORK_DIR}/UnmanagedCodexMetadataProject"
UNMANAGED_CODEX_META="${WORK_DIR}/UnmanagedCodexMetadata"
mkdir -p "${UNMANAGED_CODEX_META_PROJECT}" "${UNMANAGED_CODEX_META}/UECommandForge"
cp "${REPO_ROOT}/sample/UECommandForgeSample.uproject" \
  "${UNMANAGED_CODEX_META_PROJECT}/UECommandForgeSample.uproject"
printf 'UECF_PROJECT_FILE=/tmp/unmanaged.uproject\n' \
  > "${UNMANAGED_CODEX_META}/UECommandForge/uecommandforge.env"
if "${REPO_ROOT}/tools/release/install_local.sh" \
  --project "${UNMANAGED_CODEX_META_PROJECT}/UECommandForgeSample.uproject" \
  --plugin-package "${PLUGIN_ZIP}" \
  --tools-package "${TOOLS_ZIP}" \
  --codex-home "${UNMANAGED_CODEX_META}" \
  --run-commandlet-check false >/dev/null 2>&1; then
  echo "unmanaged existing Codex metadata should fail" >&2
  exit 1
fi

READONLY_AGENTS_PROJECT="${WORK_DIR}/ReadonlyAgentsProject"
READONLY_AGENTS_CODEX="${WORK_DIR}/ReadonlyAgentsCodex"
mkdir -p "${READONLY_AGENTS_PROJECT}" "${READONLY_AGENTS_CODEX}"
cp "${REPO_ROOT}/sample/UECommandForgeSample.uproject" \
  "${READONLY_AGENTS_PROJECT}/UECommandForgeSample.uproject"
printf '# read-only agents\n' > "${READONLY_AGENTS_CODEX}/AGENTS.md"
chmod 400 "${READONLY_AGENTS_CODEX}/AGENTS.md"
set +e
"${REPO_ROOT}/tools/release/install_local.sh" \
  --project "${READONLY_AGENTS_PROJECT}/UECommandForgeSample.uproject" \
  --plugin-package "${PLUGIN_ZIP}" \
  --tools-package "${TOOLS_ZIP}" \
  --codex-home "${READONLY_AGENTS_CODEX}" \
  --run-commandlet-check false >/dev/null 2>&1
READONLY_STATUS=$?
set -e
chmod 600 "${READONLY_AGENTS_CODEX}/AGENTS.md"
if [ "${READONLY_STATUS}" -eq 0 ]; then
  echo "read-only Codex AGENTS.md should fail before install targets are created" >&2
  exit 1
fi
test ! -e "${READONLY_AGENTS_PROJECT}/Plugins/UECommandForge"
test ! -e "${READONLY_AGENTS_PROJECT}/Saved/UECommandForge"
test ! -e "${READONLY_AGENTS_PROJECT}/UECommandForge"
test ! -e "${READONLY_AGENTS_CODEX}/UECommandForge"
test ! -e "${READONLY_AGENTS_CODEX}/UECommandForge/tools"
test ! -e "${READONLY_AGENTS_CODEX}/UECommandForge/uecommandforge-installed.json"

BROKEN_AGENTS_PROJECT="${WORK_DIR}/BrokenAgentsProject"
BROKEN_AGENTS_CODEX="${WORK_DIR}/BrokenAgentsCodex"
mkdir -p "${BROKEN_AGENTS_PROJECT}" "${BROKEN_AGENTS_CODEX}"
cp "${REPO_ROOT}/sample/UECommandForgeSample.uproject" \
  "${BROKEN_AGENTS_PROJECT}/UECommandForgeSample.uproject"
cat > "${BROKEN_AGENTS_CODEX}/AGENTS.md" <<'AGENTS'
# 개인 Codex 지침

이 내용은 깨진 마커 검사 후에도 보존되어야 한다.

<!-- BEGIN UECOMMANDFORGE CODEX INSTRUCTIONS -->
## 닫히지 않은 UECommandForge 지침

- OLD_UNCLOSED_RULE_SHOULD_REMAIN_ON_FAILURE
AGENTS
set +e
"${REPO_ROOT}/tools/release/install_local.sh" \
  --project "${BROKEN_AGENTS_PROJECT}/UECommandForgeSample.uproject" \
  --plugin-package "${PLUGIN_ZIP}" \
  --tools-package "${TOOLS_ZIP}" \
  --codex-home "${BROKEN_AGENTS_CODEX}" \
  --run-commandlet-check false >/dev/null 2>&1
BROKEN_AGENTS_STATUS=$?
set -e
if [ "${BROKEN_AGENTS_STATUS}" -eq 0 ]; then
  echo "broken Codex AGENTS.md managed markers should fail before install targets are created" >&2
  exit 1
fi
grep -q '이 내용은 깨진 마커 검사 후에도 보존되어야 한다.' "${BROKEN_AGENTS_CODEX}/AGENTS.md"
grep -q 'OLD_UNCLOSED_RULE_SHOULD_REMAIN_ON_FAILURE' "${BROKEN_AGENTS_CODEX}/AGENTS.md"
test ! -e "${BROKEN_AGENTS_PROJECT}/Plugins/UECommandForge"
test ! -e "${BROKEN_AGENTS_PROJECT}/Saved/UECommandForge"
test ! -e "${BROKEN_AGENTS_PROJECT}/UECommandForge"
test ! -e "${BROKEN_AGENTS_CODEX}/UECommandForge"

NOWRITE_CODEX_PROJECT="${WORK_DIR}/NoWriteCodexProject"
NOWRITE_CODEX_HOME="${WORK_DIR}/NoWriteCodexHome"
mkdir -p "${NOWRITE_CODEX_PROJECT}" "${NOWRITE_CODEX_HOME}"
cp "${REPO_ROOT}/sample/UECommandForgeSample.uproject" \
  "${NOWRITE_CODEX_PROJECT}/UECommandForgeSample.uproject"
printf '# writable agents in non-writable dir\n' > "${NOWRITE_CODEX_HOME}/AGENTS.md"
chmod 600 "${NOWRITE_CODEX_HOME}/AGENTS.md"
chmod 500 "${NOWRITE_CODEX_HOME}"
set +e
"${REPO_ROOT}/tools/release/install_local.sh" \
  --project "${NOWRITE_CODEX_PROJECT}/UECommandForgeSample.uproject" \
  --plugin-package "${PLUGIN_ZIP}" \
  --tools-package "${TOOLS_ZIP}" \
  --codex-home "${NOWRITE_CODEX_HOME}" \
  --run-commandlet-check false >/dev/null 2>&1
NOWRITE_STATUS=$?
set -e
chmod 700 "${NOWRITE_CODEX_HOME}"
if [ "${NOWRITE_STATUS}" -eq 0 ]; then
  echo "non-writable Codex home should fail before install targets are created" >&2
  exit 1
fi
test ! -e "${NOWRITE_CODEX_PROJECT}/Plugins/UECommandForge"
test ! -e "${NOWRITE_CODEX_PROJECT}/Saved/UECommandForge"
test ! -e "${NOWRITE_CODEX_PROJECT}/UECommandForge"
test ! -e "${NOWRITE_CODEX_HOME}/UECommandForge"

MISMATCH_TOOLS_OUT="${WORK_DIR}/MismatchedToolsPackage"
mkdir -p "${MISMATCH_TOOLS_OUT}"
"${REPO_ROOT}/tools/release/package_tools.sh" \
  --version "9.9.9-review" \
  --channel install-smoke \
  --out-dir "${MISMATCH_TOOLS_OUT}"
if "${REPO_ROOT}/tools/release/install_local.sh" \
  --project "${PROJECT_FILE}" \
  --plugin-package "${PLUGIN_ZIP}" \
  --tools-package "${MISMATCH_TOOLS_OUT}/UECommandForge-9.9.9-review-Tools.zip" \
  --codex-home "${WORK_DIR}/MismatchedToolsCodex" \
  --run-commandlet-check false >/dev/null 2>&1; then
  echo "mismatched plugin/tools release versions should fail" >&2
  exit 1
fi

MISMATCH_CHANNEL_TOOLS_OUT="${WORK_DIR}/MismatchedChannelToolsPackage"
mkdir -p "${MISMATCH_CHANNEL_TOOLS_OUT}"
"${REPO_ROOT}/tools/release/package_tools.sh" \
  --version "${VERSION}" \
  --channel different-channel \
  --out-dir "${MISMATCH_CHANNEL_TOOLS_OUT}"
if "${REPO_ROOT}/tools/release/install_local.sh" \
  --project "${PROJECT_FILE}" \
  --plugin-package "${PLUGIN_ZIP}" \
  --tools-package "${MISMATCH_CHANNEL_TOOLS_OUT}/UECommandForge-${VERSION}-Tools.zip" \
  --codex-home "${WORK_DIR}/MismatchedChannelToolsCodex" \
  --run-commandlet-check false >/dev/null 2>&1; then
  echo "mismatched plugin/tools release channels should fail" >&2
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
