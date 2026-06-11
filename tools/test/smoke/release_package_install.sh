#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
# shellcheck disable=SC1091
source "${REPO_ROOT}/tools/release/common.sh"
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

is_windows_bash() {
  uecf_is_windows_bash
}

to_windows_path() {
  cygpath -w "$1"
}

deny_directory_writes() {
  local path="$1"
  if is_windows_bash; then
    local windows_path
    windows_path="$(to_windows_path "${path}")"
    powershell.exe -NoProfile -ExecutionPolicy Bypass -Command \
      '& {
        param([string]$Path)
        $ErrorActionPreference = "Stop"
        $Sid = [System.Security.Principal.WindowsIdentity]::GetCurrent().User.Value
        $Ace = "*" + $Sid + ":(OI)(CI)(W)"
        & icacls.exe $Path /deny $Ace | Out-Null
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
      }' \
      "${windows_path}"
  else
    chmod 500 "${path}"
  fi
}

restore_directory_writes() {
  local path="$1"
  if is_windows_bash; then
    local windows_path
    windows_path="$(to_windows_path "${path}")"
    powershell.exe -NoProfile -ExecutionPolicy Bypass -Command \
      '& {
        param([string]$Path)
        $ErrorActionPreference = "Stop"
        $Sid = [System.Security.Principal.WindowsIdentity]::GetCurrent().User.Value
        $Principal = "*" + $Sid
        & icacls.exe $Path /remove:d $Principal | Out-Null
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        & icacls.exe $Path /inheritance:e | Out-Null
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
      }' \
      "${windows_path}"
  else
    chmod 700 "${path}"
  fi
}

path_is_directory_link() {
  uecf_path_is_reparse_point "$1"
}

shell_quote() {
  local value="$1"
  printf "'%s'" "$(printf '%s' "${value}" | sed "s/'/'\\\\''/g")"
}

create_directory_link() {
  local target="$1"
  local link="$2"
  if ln -s "${target}" "${link}" 2>/dev/null; then
    if path_is_directory_link "${link}"; then
      return 0
    fi
    rm -rf "${link}"
  fi

  if is_windows_bash; then
    local windows_target
    local windows_link
    windows_target="$(to_windows_path "${target}")"
    windows_link="$(to_windows_path "${link}")"
    powershell.exe -NoProfile -ExecutionPolicy Bypass -Command \
      '& {
        param([string]$Link, [string]$Target)
        $ErrorActionPreference = "Stop"
        try {
          New-Item -ItemType SymbolicLink -Path $Link -Target $Target -Force | Out-Null
        } catch {
          New-Item -ItemType Junction -Path $Link -Target $Target -Force | Out-Null
        }
      }' \
      "${windows_link}" "${windows_target}"
    path_is_directory_link "${link}"
    return $?
  fi

  return 1
}

remove_directory_fixture() {
  local path="$1"
  if is_windows_bash; then
    local windows_path
    windows_path="$(to_windows_path "${path}")"
    powershell.exe -NoProfile -ExecutionPolicy Bypass -Command \
      '& {
        param([string]$Path)
        $ErrorActionPreference = "Stop"
        if (Test-Path -LiteralPath $Path) {
          Remove-Item -LiteralPath $Path -Recurse -Force
        }
      }' \
      "${windows_path}"
  else
    rm -rf "${path}"
  fi
}

NOWRITE_DENIED=false
cleanup_permissions() {
  if [ "${NOWRITE_DENIED}" = true ] && [ -n "${NOWRITE_CODEX_HOME:-}" ]; then
    restore_directory_writes "${NOWRITE_CODEX_HOME}" || true
    NOWRITE_DENIED=false
  fi
}
trap cleanup_permissions EXIT

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
test -f "${CODEX_HOME}/UECommandForge/tools/ue/set_blueprint_defaults.sh"
test -f "${CODEX_HOME}/UECommandForge/tools/ue/set_blueprint_defaults.bat"
test -f "${CODEX_HOME}/UECommandForge/specs/policies/assets.policy.json"
test -f "${CODEX_HOME}/UECommandForge/specs/codex/unreal-automation-agents.md"
test -f "${CODEX_HOME}/UECommandForge/specs/examples/blueprint_defaults.json"
test -f "${CODEX_HOME}/skills/uecommandforge/SKILL.md"
grep -q '^generate_cpp_class_batch$' "${CODEX_HOME}/skills/uecommandforge/SKILL.md"
grep -q '^create_blueprint_batch$' "${CODEX_HOME}/skills/uecommandforge/SKILL.md"
grep -q '~/.codex/UECommandForge/specs/codex/unreal-automation-agents.md' \
  "${CODEX_HOME}/skills/uecommandforge/SKILL.md"
test -x "${CODEX_HOME}/UECommandForge/tools/ue/run_commandlet.sh"
test -x "${CODEX_HOME}/UECommandForge/tools/ue/set_blueprint_defaults.sh"
test -f "${CODEX_HOME}/UECommandForge/uecommandforge.env"
test -f "${CODEX_HOME}/UECommandForge/uecommandforge.env.sh"
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
grep -q '자동화 작업 전에는 Codex skill `uecommandforge`를 사용한다.' "${CODEX_HOME}/AGENTS.md"
! grep -q '^## 3\. 임시 파일 및 자동화 산출물 규칙' "${CODEX_HOME}/AGENTS.md"
MANAGED_BLOCK_LINES="$(awk '
  /BEGIN UECOMMANDFORGE CODEX INSTRUCTIONS/ { in_block = 1; next }
  /END UECOMMANDFORGE CODEX INSTRUCTIONS/ { in_block = 0 }
  in_block == 1 { count++ }
  END { print count + 0 }
' "${CODEX_HOME}/AGENTS.md")"
test "${MANAGED_BLOCK_LINES}" -le 30
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

grep -Fqx "UECF_PROJECT_FILE=${PROJECT_FILE}" \
  "${CODEX_HOME}/UECommandForge/uecommandforge.env"
grep -Fqx "CODEX_HOME=${CODEX_HOME}" \
  "${CODEX_HOME}/UECommandForge/uecommandforge.env"
grep -Fqx "export UECF_PROJECT_FILE=$(shell_quote "${PROJECT_FILE}")" \
  "${CODEX_HOME}/UECommandForge/uecommandforge.env.sh"
grep -Fqx "export CODEX_HOME=$(shell_quote "${CODEX_HOME}")" \
  "${CODEX_HOME}/UECommandForge/uecommandforge.env.sh"
sh -c '. "$1"; test "$UECF_PROJECT_FILE" = "$2"; test "$CODEX_HOME" = "$3"' \
  _ \
  "${CODEX_HOME}/UECommandForge/uecommandforge.env.sh" \
  "${PROJECT_FILE}" \
  "${CODEX_HOME}"

"${REPO_ROOT}/tools/release/uninstall.sh" \
  --project "${PROJECT_FILE}" \
  --codex-home "${CODEX_HOME}"

test ! -e "${PROJECT_DIR}/Plugins/UECommandForge"
test -e "${CODEX_HOME}/UECommandForge/tools"
test -e "${CODEX_HOME}/UECommandForge/specs"
test -e "${CODEX_HOME}/skills/uecommandforge/SKILL.md"
test -f "${CODEX_HOME}/UECommandForge/uecommandforge.env"
test -f "${CODEX_HOME}/UECommandForge/uecommandforge.env.sh"
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
test ! -e "${CODEX_HOME}/skills/uecommandforge"
test ! -e "${CODEX_HOME}/UECommandForge/uecommandforge.env"
test ! -e "${CODEX_HOME}/UECommandForge/uecommandforge.env.sh"
test ! -e "${CODEX_HOME}/UECommandForge/uecommandforge-installed.json"
test ! -e "${PROJECT_DIR}/UECommandForge/uecommandforge-project.json"
test -f "${PROJECT_DIR}/Saved/UECommandForge/uninstall.log"

REVERSED_SOURCE_PROJECT="${WORK_DIR}/ReversedSourceProject"
REVERSED_SOURCE_CODEX="${WORK_DIR}/ReversedSourceCodex"
REVERSED_SOURCE_OUT="${WORK_DIR}/ReversedSourceToolsPackage"
REVERSED_SOURCE_PACKAGE="${REVERSED_SOURCE_OUT}/package"
mkdir -p "${REVERSED_SOURCE_PROJECT}" "${REVERSED_SOURCE_CODEX}" "${REVERSED_SOURCE_PACKAGE}"
cp "${REPO_ROOT}/sample/UECommandForgeSample.uproject" \
  "${REVERSED_SOURCE_PROJECT}/UECommandForgeSample.uproject"
unzip -q "${TOOLS_ZIP}" -d "${REVERSED_SOURCE_PACKAGE}"
awk '
  /BEGIN UECOMMANDFORGE MANAGED CONTENT/ {
    print "<!-- END UECOMMANDFORGE MANAGED CONTENT -->"
    next
  }
  /END UECOMMANDFORGE MANAGED CONTENT/ {
    print "<!-- BEGIN UECOMMANDFORGE MANAGED CONTENT -->"
    next
  }
  { print }
' "${REVERSED_SOURCE_PACKAGE}/specs/codex/unreal-automation-agents.md" \
  > "${REVERSED_SOURCE_PACKAGE}/specs/codex/unreal-automation-agents.md.tmp"
mv "${REVERSED_SOURCE_PACKAGE}/specs/codex/unreal-automation-agents.md.tmp" \
  "${REVERSED_SOURCE_PACKAGE}/specs/codex/unreal-automation-agents.md"
"${REPO_ROOT}/tools/release/write_manifest.sh" \
  --package-root "${REVERSED_SOURCE_PACKAGE}" \
  --version "${VERSION}" \
  --channel install-smoke \
  --package-type tools \
  --install-command "./install-uecommandforge.sh" \
  --install-command "install-uecommandforge.bat" \
  --install-command "install-uecommandforge.ps1" \
  --output "${REVERSED_SOURCE_PACKAGE}/uecommandforge-manifest.json"
(
  cd "${REVERSED_SOURCE_PACKAGE}"
  uecf_create_zip "${REVERSED_SOURCE_OUT}/UECommandForge-${VERSION}-Tools.zip" \
    tools specs skills \
    install-uecommandforge.sh install-uecommandforge.bat install-uecommandforge.ps1 \
    uecommandforge-manifest.json install.md release-notes.md validation-report.json
)
uecf_write_checksums_file \
  "${REVERSED_SOURCE_OUT}/UECommandForge-${VERSION}-Tools.zip" \
  "${REVERSED_SOURCE_OUT}/checksums.txt" \
  "release_package_install_smoke"
if "${REPO_ROOT}/tools/release/install_local.sh" \
  --project "${REVERSED_SOURCE_PROJECT}/UECommandForgeSample.uproject" \
  --plugin-package "${PLUGIN_ZIP}" \
  --tools-package "${REVERSED_SOURCE_OUT}/UECommandForge-${VERSION}-Tools.zip" \
  --codex-home "${REVERSED_SOURCE_CODEX}" \
  --run-commandlet-check false >/dev/null 2>&1; then
  echo "reversed instruction source markers should fail before install targets are created" >&2
  exit 1
fi
test ! -e "${REVERSED_SOURCE_PROJECT}/Plugins/UECommandForge"
test ! -e "${REVERSED_SOURCE_PROJECT}/Saved/UECommandForge"
test ! -e "${REVERSED_SOURCE_PROJECT}/UECommandForge"
test ! -e "${REVERSED_SOURCE_CODEX}/UECommandForge"

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

REVERSED_AGENTS_PROJECT="${WORK_DIR}/ReversedAgentsProject"
REVERSED_AGENTS_CODEX="${WORK_DIR}/ReversedAgentsCodex"
mkdir -p "${REVERSED_AGENTS_PROJECT}" "${REVERSED_AGENTS_CODEX}"
cp "${REPO_ROOT}/sample/UECommandForgeSample.uproject" \
  "${REVERSED_AGENTS_PROJECT}/UECommandForgeSample.uproject"
cat > "${REVERSED_AGENTS_CODEX}/AGENTS.md" <<'AGENTS'
# 개인 Codex 지침

<!-- END UECOMMANDFORGE CODEX INSTRUCTIONS -->
이 내용은 역순 마커 검사 후에도 보존되어야 한다.
<!-- BEGIN UECOMMANDFORGE CODEX INSTRUCTIONS -->
AGENTS
if "${REPO_ROOT}/tools/release/install_local.sh" \
  --project "${REVERSED_AGENTS_PROJECT}/UECommandForgeSample.uproject" \
  --plugin-package "${PLUGIN_ZIP}" \
  --tools-package "${TOOLS_ZIP}" \
  --codex-home "${REVERSED_AGENTS_CODEX}" \
  --run-commandlet-check false >/dev/null 2>&1; then
  echo "reversed Codex AGENTS.md managed markers should fail before install targets are created" >&2
  exit 1
fi
grep -q '이 내용은 역순 마커 검사 후에도 보존되어야 한다.' "${REVERSED_AGENTS_CODEX}/AGENTS.md"
test ! -e "${REVERSED_AGENTS_PROJECT}/Plugins/UECommandForge"
test ! -e "${REVERSED_AGENTS_PROJECT}/Saved/UECommandForge"
test ! -e "${REVERSED_AGENTS_PROJECT}/UECommandForge"
test ! -e "${REVERSED_AGENTS_CODEX}/UECommandForge"

NOWRITE_CODEX_PROJECT="${WORK_DIR}/NoWriteCodexProject"
NOWRITE_CODEX_HOME="${WORK_DIR}/NoWriteCodexHome"
mkdir -p "${NOWRITE_CODEX_PROJECT}" "${NOWRITE_CODEX_HOME}"
cp "${REPO_ROOT}/sample/UECommandForgeSample.uproject" \
  "${NOWRITE_CODEX_PROJECT}/UECommandForgeSample.uproject"
printf '# writable agents in non-writable dir\n' > "${NOWRITE_CODEX_HOME}/AGENTS.md"
chmod 600 "${NOWRITE_CODEX_HOME}/AGENTS.md"
deny_directory_writes "${NOWRITE_CODEX_HOME}"
NOWRITE_DENIED=true
set +e
"${REPO_ROOT}/tools/release/install_local.sh" \
  --project "${NOWRITE_CODEX_PROJECT}/UECommandForgeSample.uproject" \
  --plugin-package "${PLUGIN_ZIP}" \
  --tools-package "${TOOLS_ZIP}" \
  --codex-home "${NOWRITE_CODEX_HOME}" \
  --run-commandlet-check false >/dev/null 2>&1
NOWRITE_STATUS=$?
set -e
restore_directory_writes "${NOWRITE_CODEX_HOME}"
NOWRITE_DENIED=false
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

MISMATCH_TYPE_PLUGIN_OUT="${WORK_DIR}/MismatchedTypePluginPackage"
MISMATCH_TYPE_PLUGIN_PACKAGE="${MISMATCH_TYPE_PLUGIN_OUT}/package"
mkdir -p "${MISMATCH_TYPE_PLUGIN_PACKAGE}"
unzip -q "${PLUGIN_ZIP}" -d "${MISMATCH_TYPE_PLUGIN_PACKAGE}"
jq '.package_type = "tools"' \
  "${MISMATCH_TYPE_PLUGIN_PACKAGE}/uecommandforge-manifest.json" \
  > "${MISMATCH_TYPE_PLUGIN_PACKAGE}/uecommandforge-manifest.json.tmp"
mv "${MISMATCH_TYPE_PLUGIN_PACKAGE}/uecommandforge-manifest.json.tmp" \
  "${MISMATCH_TYPE_PLUGIN_PACKAGE}/uecommandforge-manifest.json"
(
  cd "${MISMATCH_TYPE_PLUGIN_PACKAGE}"
  uecf_create_zip "${MISMATCH_TYPE_PLUGIN_OUT}/$(basename "${PLUGIN_ZIP}")" ./*
)
uecf_write_checksums_file \
  "${MISMATCH_TYPE_PLUGIN_OUT}/$(basename "${PLUGIN_ZIP}")" \
  "${MISMATCH_TYPE_PLUGIN_OUT}/checksums.txt" \
  "release_package_install_smoke"
if "${REPO_ROOT}/tools/release/install_local.sh" \
  --project "${PROJECT_FILE}" \
  --plugin-package "${MISMATCH_TYPE_PLUGIN_OUT}/$(basename "${PLUGIN_ZIP}")" \
  --tools-package "${TOOLS_ZIP}" \
  --codex-home "${WORK_DIR}/MismatchedTypePluginCodex" \
  --run-commandlet-check false >/dev/null 2>&1; then
  echo "mismatched plugin package type should fail" >&2
  exit 1
fi

MISMATCH_TYPE_TOOLS_OUT="${WORK_DIR}/MismatchedTypeToolsPackage"
MISMATCH_TYPE_TOOLS_PACKAGE="${MISMATCH_TYPE_TOOLS_OUT}/package"
mkdir -p "${MISMATCH_TYPE_TOOLS_PACKAGE}"
unzip -q "${TOOLS_ZIP}" -d "${MISMATCH_TYPE_TOOLS_PACKAGE}"
jq '.package_type = "plugin"' \
  "${MISMATCH_TYPE_TOOLS_PACKAGE}/uecommandforge-manifest.json" \
  > "${MISMATCH_TYPE_TOOLS_PACKAGE}/uecommandforge-manifest.json.tmp"
mv "${MISMATCH_TYPE_TOOLS_PACKAGE}/uecommandforge-manifest.json.tmp" \
  "${MISMATCH_TYPE_TOOLS_PACKAGE}/uecommandforge-manifest.json"
(
  cd "${MISMATCH_TYPE_TOOLS_PACKAGE}"
  uecf_create_zip "${MISMATCH_TYPE_TOOLS_OUT}/UECommandForge-${VERSION}-Tools.zip" ./*
)
uecf_write_checksums_file \
  "${MISMATCH_TYPE_TOOLS_OUT}/UECommandForge-${VERSION}-Tools.zip" \
  "${MISMATCH_TYPE_TOOLS_OUT}/checksums.txt" \
  "release_package_install_smoke"
if "${REPO_ROOT}/tools/release/install_local.sh" \
  --project "${PROJECT_FILE}" \
  --plugin-package "${PLUGIN_ZIP}" \
  --tools-package "${MISMATCH_TYPE_TOOLS_OUT}/UECommandForge-${VERSION}-Tools.zip" \
  --codex-home "${WORK_DIR}/MismatchedTypeToolsCodex" \
  --run-commandlet-check false >/dev/null 2>&1; then
  echo "mismatched tools package type should fail" >&2
  exit 1
fi

ENV_QUOTE_PROJECT="${WORK_DIR}/Env Quote \$Project"
ENV_QUOTE_CODEX="${WORK_DIR}/Env Quote \$Codex"
mkdir -p "${ENV_QUOTE_PROJECT}" "${ENV_QUOTE_CODEX}"
cp "${REPO_ROOT}/sample/UECommandForgeSample.uproject" \
  "${ENV_QUOTE_PROJECT}/UECommandForgeSample.uproject"
"${REPO_ROOT}/tools/release/install_local.sh" \
  --project "${ENV_QUOTE_PROJECT}/UECommandForgeSample.uproject" \
  --plugin-package "${PLUGIN_ZIP}" \
  --tools-package "${TOOLS_ZIP}" \
  --codex-home "${ENV_QUOTE_CODEX}" \
  --run-commandlet-check false >/dev/null
grep -Fqx "UECF_PROJECT_FILE=${ENV_QUOTE_PROJECT}/UECommandForgeSample.uproject" \
  "${ENV_QUOTE_CODEX}/UECommandForge/uecommandforge.env"
grep -Fqx "CODEX_HOME=${ENV_QUOTE_CODEX}" \
  "${ENV_QUOTE_CODEX}/UECommandForge/uecommandforge.env"
grep -Fqx "export UECF_PROJECT_FILE=$(shell_quote "${ENV_QUOTE_PROJECT}/UECommandForgeSample.uproject")" \
  "${ENV_QUOTE_CODEX}/UECommandForge/uecommandforge.env.sh"
grep -Fqx "export CODEX_HOME=$(shell_quote "${ENV_QUOTE_CODEX}")" \
  "${ENV_QUOTE_CODEX}/UECommandForge/uecommandforge.env.sh"
sh -c '. "$1"; test "$UECF_PROJECT_FILE" = "$2"; test "$CODEX_HOME" = "$3"' \
  _ \
  "${ENV_QUOTE_CODEX}/UECommandForge/uecommandforge.env.sh" \
  "${ENV_QUOTE_PROJECT}/UECommandForgeSample.uproject" \
  "${ENV_QUOTE_CODEX}"

SYMLINK_PROJECT="${WORK_DIR}/SymlinkProject"
SYMLINK_TARGET="${WORK_DIR}/SymlinkTarget"
mkdir -p "${SYMLINK_PROJECT}" "${SYMLINK_TARGET}"
cp "${REPO_ROOT}/sample/UECommandForgeSample.uproject" "${SYMLINK_PROJECT}/UECommandForgeSample.uproject"
if ! create_directory_link "${SYMLINK_TARGET}" "${SYMLINK_PROJECT}/Plugins"; then
  echo "could not create symlinked Plugins directory fixture" >&2
  exit 1
fi
if "${REPO_ROOT}/tools/release/install_local.sh" \
  --project "${SYMLINK_PROJECT}/UECommandForgeSample.uproject" \
  --plugin-package "${PLUGIN_ZIP}" \
  --tools-package "${TOOLS_ZIP}" \
  --codex-home "${WORK_DIR}/SymlinkCodex" \
  --run-commandlet-check false >/dev/null 2>&1; then
  echo "symlinked Plugins directory should fail" >&2
  exit 1
fi

REPARSE_CODEX_PROJECT="${WORK_DIR}/ReparseCodexHomeProject"
REPARSE_CODEX_TARGET="${WORK_DIR}/ReparseCodexHomeTarget"
REPARSE_CODEX_HOME="${WORK_DIR}/ReparseCodexHome"
mkdir -p "${REPARSE_CODEX_PROJECT}" "${REPARSE_CODEX_TARGET}"
cp "${REPO_ROOT}/sample/UECommandForgeSample.uproject" \
  "${REPARSE_CODEX_PROJECT}/UECommandForgeSample.uproject"
if ! create_directory_link "${REPARSE_CODEX_TARGET}" "${REPARSE_CODEX_HOME}"; then
  echo "could not create reparse Codex home fixture" >&2
  exit 1
fi
if "${REPO_ROOT}/tools/release/install_local.sh" \
  --project "${REPARSE_CODEX_PROJECT}/UECommandForgeSample.uproject" \
  --plugin-package "${PLUGIN_ZIP}" \
  --tools-package "${TOOLS_ZIP}" \
  --codex-home "${REPARSE_CODEX_HOME}" \
  --run-commandlet-check false >/dev/null 2>&1; then
  echo "reparse Codex home should fail" >&2
  exit 1
fi
test ! -e "${REPARSE_CODEX_PROJECT}/Plugins/UECommandForge"
test ! -e "${REPARSE_CODEX_PROJECT}/Saved/UECommandForge"
test ! -e "${REPARSE_CODEX_PROJECT}/UECommandForge"
test ! -e "${REPARSE_CODEX_TARGET}/UECommandForge"

REPARSE_PARENT_PROJECT="${WORK_DIR}/ReparseCodexParentProject"
REPARSE_PARENT_TARGET="${WORK_DIR}/ReparseCodexParentTarget"
REPARSE_PARENT_LINK="${WORK_DIR}/ReparseCodexParent"
REPARSE_PARENT_CODEX_HOME="${REPARSE_PARENT_LINK}/NestedCodexHome"
mkdir -p "${REPARSE_PARENT_PROJECT}" "${REPARSE_PARENT_TARGET}"
cp "${REPO_ROOT}/sample/UECommandForgeSample.uproject" \
  "${REPARSE_PARENT_PROJECT}/UECommandForgeSample.uproject"
if ! create_directory_link "${REPARSE_PARENT_TARGET}" "${REPARSE_PARENT_LINK}"; then
  echo "could not create reparse Codex parent fixture" >&2
  exit 1
fi
if "${REPO_ROOT}/tools/release/install_local.sh" \
  --project "${REPARSE_PARENT_PROJECT}/UECommandForgeSample.uproject" \
  --plugin-package "${PLUGIN_ZIP}" \
  --tools-package "${TOOLS_ZIP}" \
  --codex-home "${REPARSE_PARENT_CODEX_HOME}" \
  --run-commandlet-check false >/dev/null 2>&1; then
  echo "Codex home below reparse parent should fail" >&2
  exit 1
fi
test ! -e "${REPARSE_PARENT_PROJECT}/Plugins/UECommandForge"
test ! -e "${REPARSE_PARENT_PROJECT}/Saved/UECommandForge"
test ! -e "${REPARSE_PARENT_PROJECT}/UECommandForge"
test ! -e "${REPARSE_PARENT_TARGET}/NestedCodexHome"

REPARSE_PROJECT_TARGET="${WORK_DIR}/ReparseProjectTarget"
REPARSE_PROJECT_PARENT="${WORK_DIR}/ReparseProjectParent"
REPARSE_PROJECT_DIR="${REPARSE_PROJECT_PARENT}/Project"
REPARSE_PROJECT_CODEX="${WORK_DIR}/ReparseProjectCodex"
mkdir -p "${REPARSE_PROJECT_TARGET}" "${REPARSE_PROJECT_CODEX}"
if ! create_directory_link "${REPARSE_PROJECT_TARGET}" "${REPARSE_PROJECT_PARENT}"; then
  echo "could not create reparse project parent fixture" >&2
  exit 1
fi
mkdir -p "${REPARSE_PROJECT_DIR}"
cp "${REPO_ROOT}/sample/UECommandForgeSample.uproject" \
  "${REPARSE_PROJECT_DIR}/UECommandForgeSample.uproject"
if "${REPO_ROOT}/tools/release/install_local.sh" \
  --project "${REPARSE_PROJECT_DIR}/UECommandForgeSample.uproject" \
  --plugin-package "${PLUGIN_ZIP}" \
  --tools-package "${TOOLS_ZIP}" \
  --codex-home "${REPARSE_PROJECT_CODEX}" \
  --run-commandlet-check false >/dev/null 2>&1; then
  echo "project below reparse parent should fail" >&2
  exit 1
fi
test ! -e "${REPARSE_PROJECT_TARGET}/Project/Plugins/UECommandForge"
test ! -e "${REPARSE_PROJECT_TARGET}/Project/Saved/UECommandForge"
test ! -e "${REPARSE_PROJECT_TARGET}/Project/UECommandForge"
test ! -e "${REPARSE_PROJECT_CODEX}/UECommandForge"

RELATIVE_REPARSE_ROOT="${WORK_DIR}/RelativeReparseRoot"
RELATIVE_REPARSE_TARGET="${WORK_DIR}/RelativeReparseTarget"
RELATIVE_REPARSE_CODEX_TARGET="${WORK_DIR}/RelativeReparseCodexTarget"
mkdir -p "${RELATIVE_REPARSE_TARGET}" "${RELATIVE_REPARSE_CODEX_TARGET}"
if ! create_directory_link "${RELATIVE_REPARSE_TARGET}" "${RELATIVE_REPARSE_ROOT}"; then
  echo "could not create relative reparse cwd fixture" >&2
  exit 1
fi
mkdir -p "${RELATIVE_REPARSE_ROOT}/Project" "${RELATIVE_REPARSE_ROOT}/CodexHome"
cp "${REPO_ROOT}/sample/UECommandForgeSample.uproject" \
  "${RELATIVE_REPARSE_ROOT}/Project/UECommandForgeSample.uproject"
(
  cd "${RELATIVE_REPARSE_ROOT}"
  if "${REPO_ROOT}/tools/release/install_local.sh" \
    --project "Project/UECommandForgeSample.uproject" \
    --plugin-package "${PLUGIN_ZIP}" \
    --tools-package "${TOOLS_ZIP}" \
    --codex-home "CodexHome" \
    --run-commandlet-check false >/dev/null 2>&1; then
    echo "relative install below reparse cwd should fail" >&2
    exit 1
  fi
)
test ! -e "${RELATIVE_REPARSE_TARGET}/Project/Plugins/UECommandForge"
test ! -e "${RELATIVE_REPARSE_TARGET}/Project/Saved/UECommandForge"
test ! -e "${RELATIVE_REPARSE_TARGET}/CodexHome/UECommandForge"

EXISTING_CHILD_REPARSE_PROJECT="${WORK_DIR}/ExistingChildReparseProject"
EXISTING_CHILD_REPARSE_CODEX="${WORK_DIR}/ExistingChildReparseCodex"
EXISTING_CHILD_REPARSE_TARGET="${WORK_DIR}/ExistingChildReparseTarget"
mkdir -p "${EXISTING_CHILD_REPARSE_PROJECT}" "${EXISTING_CHILD_REPARSE_CODEX}" "${EXISTING_CHILD_REPARSE_TARGET}"
cp "${REPO_ROOT}/sample/UECommandForgeSample.uproject" \
  "${EXISTING_CHILD_REPARSE_PROJECT}/UECommandForgeSample.uproject"
"${REPO_ROOT}/tools/release/install_local.sh" \
  --project "${EXISTING_CHILD_REPARSE_PROJECT}/UECommandForgeSample.uproject" \
  --plugin-package "${PLUGIN_ZIP}" \
  --tools-package "${TOOLS_ZIP}" \
  --codex-home "${EXISTING_CHILD_REPARSE_CODEX}" \
  --run-commandlet-check false >/dev/null
printf 'preserve existing install child target\n' > "${EXISTING_CHILD_REPARSE_TARGET}/sentinel.txt"
remove_directory_fixture "${EXISTING_CHILD_REPARSE_PROJECT}/Plugins/UECommandForge/Binaries"
if ! create_directory_link "${EXISTING_CHILD_REPARSE_TARGET}" \
  "${EXISTING_CHILD_REPARSE_PROJECT}/Plugins/UECommandForge/Binaries"; then
  echo "could not create existing install child reparse fixture" >&2
  exit 1
fi
if "${REPO_ROOT}/tools/release/install_local.sh" \
  --project "${EXISTING_CHILD_REPARSE_PROJECT}/UECommandForgeSample.uproject" \
  --plugin-package "${PLUGIN_ZIP}" \
  --tools-package "${TOOLS_ZIP}" \
  --codex-home "${EXISTING_CHILD_REPARSE_CODEX}" \
  --backup false \
  --run-commandlet-check false >/dev/null 2>&1; then
  echo "existing install target with reparse child should fail" >&2
  exit 1
fi
grep -q 'preserve existing install child target' "${EXISTING_CHILD_REPARSE_TARGET}/sentinel.txt"

BACKUP_REPARSE_PROJECT="${WORK_DIR}/BackupReparseProject"
BACKUP_REPARSE_CODEX="${WORK_DIR}/BackupReparseCodex"
BACKUP_REPARSE_TARGET="${WORK_DIR}/BackupReparseTarget"
BACKUP_REPARSE_LINK="${BACKUP_REPARSE_PROJECT}/Saved/UECommandForge/Backups"
mkdir -p "${BACKUP_REPARSE_PROJECT}" "${BACKUP_REPARSE_CODEX}" "${BACKUP_REPARSE_TARGET}"
cp "${REPO_ROOT}/sample/UECommandForgeSample.uproject" \
  "${BACKUP_REPARSE_PROJECT}/UECommandForgeSample.uproject"
"${REPO_ROOT}/tools/release/install_local.sh" \
  --project "${BACKUP_REPARSE_PROJECT}/UECommandForgeSample.uproject" \
  --plugin-package "${PLUGIN_ZIP}" \
  --tools-package "${TOOLS_ZIP}" \
  --codex-home "${BACKUP_REPARSE_CODEX}" \
  --run-commandlet-check false >/dev/null
printf 'preserve backup target\n' > "${BACKUP_REPARSE_TARGET}/sentinel.txt"
remove_directory_fixture "${BACKUP_REPARSE_LINK}"
if ! create_directory_link "${BACKUP_REPARSE_TARGET}" "${BACKUP_REPARSE_LINK}"; then
  echo "could not create backup reparse fixture" >&2
  exit 1
fi
if "${REPO_ROOT}/tools/release/install_local.sh" \
  --project "${BACKUP_REPARSE_PROJECT}/UECommandForgeSample.uproject" \
  --plugin-package "${PLUGIN_ZIP}" \
  --tools-package "${TOOLS_ZIP}" \
  --codex-home "${BACKUP_REPARSE_CODEX}" \
  --run-commandlet-check false >/dev/null 2>&1; then
  echo "backup reparse path should fail" >&2
  exit 1
fi
grep -q 'preserve backup target' "${BACKUP_REPARSE_TARGET}/sentinel.txt"

for metadata_case in env shell_env installed project log; do
  METADATA_REPARSE_PROJECT="${WORK_DIR}/MetadataReparseProject-${metadata_case}"
  METADATA_REPARSE_CODEX="${WORK_DIR}/MetadataReparseCodex-${metadata_case}"
  METADATA_REPARSE_TARGET="${WORK_DIR}/MetadataReparseTarget-${metadata_case}"
  mkdir -p "${METADATA_REPARSE_PROJECT}" "${METADATA_REPARSE_CODEX}" "${METADATA_REPARSE_TARGET}"
  cp "${REPO_ROOT}/sample/UECommandForgeSample.uproject" \
    "${METADATA_REPARSE_PROJECT}/UECommandForgeSample.uproject"
  "${REPO_ROOT}/tools/release/install_local.sh" \
    --project "${METADATA_REPARSE_PROJECT}/UECommandForgeSample.uproject" \
    --plugin-package "${PLUGIN_ZIP}" \
    --tools-package "${TOOLS_ZIP}" \
    --codex-home "${METADATA_REPARSE_CODEX}" \
    --run-commandlet-check false >/dev/null
  printf 'preserve metadata target %s\n' "${metadata_case}" > "${METADATA_REPARSE_TARGET}/sentinel.txt"

  case "${metadata_case}" in
    env)
      METADATA_REPARSE_PATH="${METADATA_REPARSE_CODEX}/UECommandForge/uecommandforge.env"
      ;;
    shell_env)
      METADATA_REPARSE_PATH="${METADATA_REPARSE_CODEX}/UECommandForge/uecommandforge.env.sh"
      ;;
    installed)
      METADATA_REPARSE_PATH="${METADATA_REPARSE_CODEX}/UECommandForge/uecommandforge-installed.json"
      ;;
    project)
      METADATA_REPARSE_PATH="${METADATA_REPARSE_PROJECT}/UECommandForge/uecommandforge-project.json"
      ;;
    log)
      METADATA_REPARSE_PATH="${METADATA_REPARSE_PROJECT}/Saved/UECommandForge/install.log"
      ;;
  esac

  rm -f "${METADATA_REPARSE_PATH}"
  remove_directory_fixture "${METADATA_REPARSE_PATH}"
  mkdir -p "${METADATA_REPARSE_PATH}"
  if ! create_directory_link "${METADATA_REPARSE_TARGET}" "${METADATA_REPARSE_PATH}/child"; then
    echo "could not create metadata reparse fixture: ${metadata_case}" >&2
    exit 1
  fi
  if "${REPO_ROOT}/tools/release/install_local.sh" \
    --project "${METADATA_REPARSE_PROJECT}/UECommandForgeSample.uproject" \
    --plugin-package "${PLUGIN_ZIP}" \
    --tools-package "${TOOLS_ZIP}" \
    --codex-home "${METADATA_REPARSE_CODEX}" \
    --run-commandlet-check false >/dev/null 2>&1; then
    echo "metadata reparse output path should fail: ${metadata_case}" >&2
    exit 1
  fi
  grep -q "preserve metadata target ${metadata_case}" "${METADATA_REPARSE_TARGET}/sentinel.txt"
done

for metadata_case in env shell_env installed project log; do
  METADATA_HARDLINK_PROJECT="${WORK_DIR}/MetadataHardlinkProject-${metadata_case}"
  METADATA_HARDLINK_CODEX="${WORK_DIR}/MetadataHardlinkCodex-${metadata_case}"
  METADATA_HARDLINK_TARGET="${WORK_DIR}/MetadataHardlinkTarget-${metadata_case}.txt"
  METADATA_HARDLINK_BEFORE="${WORK_DIR}/MetadataHardlinkTarget-${metadata_case}.before"
  mkdir -p "${METADATA_HARDLINK_PROJECT}" "${METADATA_HARDLINK_CODEX}"
  cp "${REPO_ROOT}/sample/UECommandForgeSample.uproject" \
    "${METADATA_HARDLINK_PROJECT}/UECommandForgeSample.uproject"
  "${REPO_ROOT}/tools/release/install_local.sh" \
    --project "${METADATA_HARDLINK_PROJECT}/UECommandForgeSample.uproject" \
    --plugin-package "${PLUGIN_ZIP}" \
    --tools-package "${TOOLS_ZIP}" \
    --codex-home "${METADATA_HARDLINK_CODEX}" \
    --run-commandlet-check false >/dev/null

  case "${metadata_case}" in
    env)
      METADATA_HARDLINK_PATH="${METADATA_HARDLINK_CODEX}/UECommandForge/uecommandforge.env"
      ;;
    shell_env)
      METADATA_HARDLINK_PATH="${METADATA_HARDLINK_CODEX}/UECommandForge/uecommandforge.env.sh"
      ;;
    installed)
      METADATA_HARDLINK_PATH="${METADATA_HARDLINK_CODEX}/UECommandForge/uecommandforge-installed.json"
      ;;
    project)
      METADATA_HARDLINK_PATH="${METADATA_HARDLINK_PROJECT}/UECommandForge/uecommandforge-project.json"
      ;;
    log)
      METADATA_HARDLINK_PATH="${METADATA_HARDLINK_PROJECT}/Saved/UECommandForge/install.log"
      ;;
  esac

  cp "${METADATA_HARDLINK_PATH}" "${METADATA_HARDLINK_TARGET}"
  cp "${METADATA_HARDLINK_TARGET}" "${METADATA_HARDLINK_BEFORE}"
  rm -f "${METADATA_HARDLINK_PATH}"
  if ! ln "${METADATA_HARDLINK_TARGET}" "${METADATA_HARDLINK_PATH}"; then
    echo "could not create metadata hardlink fixture: ${metadata_case}" >&2
    exit 1
  fi
  "${REPO_ROOT}/tools/release/install_local.sh" \
    --project "${METADATA_HARDLINK_PROJECT}/UECommandForgeSample.uproject" \
    --plugin-package "${PLUGIN_ZIP}" \
    --tools-package "${TOOLS_ZIP}" \
    --codex-home "${METADATA_HARDLINK_CODEX}" \
    --run-commandlet-check false >/dev/null
  cmp "${METADATA_HARDLINK_TARGET}" "${METADATA_HARDLINK_BEFORE}"
done

UNINSTALL_CHILD_REPARSE_PROJECT="${WORK_DIR}/UninstallChildReparseProject"
UNINSTALL_CHILD_REPARSE_CODEX="${WORK_DIR}/UninstallChildReparseCodex"
UNINSTALL_CHILD_REPARSE_TARGET="${WORK_DIR}/UninstallChildReparseTarget"
mkdir -p "${UNINSTALL_CHILD_REPARSE_PROJECT}" \
  "${UNINSTALL_CHILD_REPARSE_CODEX}" \
  "${UNINSTALL_CHILD_REPARSE_TARGET}"
cp "${REPO_ROOT}/sample/UECommandForgeSample.uproject" \
  "${UNINSTALL_CHILD_REPARSE_PROJECT}/UECommandForgeSample.uproject"
"${REPO_ROOT}/tools/release/install_local.sh" \
  --project "${UNINSTALL_CHILD_REPARSE_PROJECT}/UECommandForgeSample.uproject" \
  --plugin-package "${PLUGIN_ZIP}" \
  --tools-package "${TOOLS_ZIP}" \
  --codex-home "${UNINSTALL_CHILD_REPARSE_CODEX}" \
  --run-commandlet-check false >/dev/null
printf 'preserve uninstall child target\n' > "${UNINSTALL_CHILD_REPARSE_TARGET}/sentinel.txt"
remove_directory_fixture "${UNINSTALL_CHILD_REPARSE_CODEX}/UECommandForge/tools/ue"
if ! create_directory_link "${UNINSTALL_CHILD_REPARSE_TARGET}" \
  "${UNINSTALL_CHILD_REPARSE_CODEX}/UECommandForge/tools/ue"; then
  echo "could not create uninstall child reparse fixture" >&2
  exit 1
fi
if "${REPO_ROOT}/tools/release/uninstall.sh" \
  --project "${UNINSTALL_CHILD_REPARSE_PROJECT}/UECommandForgeSample.uproject" \
  --codex-home "${UNINSTALL_CHILD_REPARSE_CODEX}" \
  --remove-codex-tools true \
  --remove-specs true >/dev/null 2>&1; then
  echo "uninstall target with reparse child should fail" >&2
  exit 1
fi
grep -q 'preserve uninstall child target' "${UNINSTALL_CHILD_REPARSE_TARGET}/sentinel.txt"

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
