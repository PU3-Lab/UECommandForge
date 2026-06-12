#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
# shellcheck disable=SC1091
source "${REPO_ROOT}/tools/release/common.sh"
WORK_DIR="${REPO_ROOT}/sample/Saved/UECommandForge/Reports/ReleasePackageInstall"
VERSION="$(jq -r '.VersionName' "${REPO_ROOT}/sample/Plugins/UECommandForge/UECommandForge.uplugin")"
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
  if [ "${NOWRITE_DENIED}" = true ] && [ -n "${NOWRITE_AGENT_HOME:-}" ]; then
    restore_directory_writes "${NOWRITE_AGENT_HOME}" || true
    NOWRITE_DENIED=false
  fi
}
trap cleanup_permissions EXIT

rm -rf "${WORK_DIR}"

# 1. 패키징 빌드 및 툴 패키징 준비
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

# ==========================================
# TEST CASE 1: 기본 설치 (Codex 프로파일)
# ==========================================
BASE_DIR="${WORK_DIR}/BaseCase"
PROJECT_DIR="${BASE_DIR}/Project"
CODEX_HOME="${BASE_DIR}/.codex"
mkdir -p "${PROJECT_DIR}" "${CODEX_HOME}"
cp "${REPO_ROOT}/sample/UECommandForgeSample.uproject" "${PROJECT_DIR}/UECommandForgeSample.uproject"
PROJECT_FILE="${PROJECT_DIR}/UECommandForgeSample.uproject"

cat > "${CODEX_HOME}/AGENTS.md" <<'AGENTS'
# 개인 Codex 지침
이 내용은 설치 후에도 보존되어야 한다.
<!-- BEGIN UECOMMANDFORGE CODEX INSTRUCTIONS -->
## 오래된 UECommandForge 지침
- OLD_UNREAL_PYTHON_RULE_SHOULD_BE_REPLACED
<!-- END UECOMMANDFORGE CODEX INSTRUCTIONS -->
AGENTS

# HOME 격리 하에 설치 실행
HOME="${BASE_DIR}" "${REPO_ROOT}/tools/release/install_local.sh" \
  --project "${PROJECT_FILE}" \
  --plugin-package "${PLUGIN_ZIP}" \
  --tools-package "${TOOLS_ZIP}" \
  --codex \
  --run-commandlet-check false

# 검증
test -f "${PROJECT_DIR}/Plugins/UECommandForge/UECommandForge.uplugin"
test -f "${PROJECT_DIR}/Plugins/UECommandForge/${RUNTIME_BINARY}"
test -f "${CODEX_HOME}/UECommandForge/tools/ue/run_commandlet.sh"
test -f "${CODEX_HOME}/skills/uecommandforge/SKILL.md"
test -f "${CODEX_HOME}/UECommandForge/uecommandforge-installed.json"
test -f "${PROJECT_DIR}/UECommandForge/uecommandforge-project.json"
test -f "${CODEX_HOME}/AGENTS.md"
grep -q '이 내용은 설치 후에도 보존되어야 한다.' "${CODEX_HOME}/AGENTS.md"
grep -q 'BEGIN UECOMMANDFORGE INSTRUCTIONS' "${CODEX_HOME}/AGENTS.md"
! grep -q 'OLD_UNREAL_PYTHON_RULE_SHOULD_BE_REPLACED' "${CODEX_HOME}/AGENTS.md"

# env 파일 source 및 AGENT_HOME 검증
source "${CODEX_HOME}/UECommandForge/uecommandforge.env.sh"
test "${AGENT_HOME}" = "${CODEX_HOME}"
test "${UECF_PROJECT_FILE}" = "${PROJECT_FILE}"

# 마커 개수 상한선 및 제거 검증
MANAGED_BLOCK_LINES="$(grep -A 100 'BEGIN UECOMMANDFORGE INSTRUCTIONS' "${CODEX_HOME}/AGENTS.md" | grep -B 100 'END UECOMMANDFORGE INSTRUCTIONS' | wc -l)"
test "${MANAGED_BLOCK_LINES}" -le 30
test "$(grep -c 'BEGIN UECOMMANDFORGE CODEX INSTRUCTIONS' "${CODEX_HOME}/AGENTS.md" || true)" -eq 0
test "$(grep -c 'BEGIN UECOMMANDFORGE INSTRUCTIONS' "${CODEX_HOME}/AGENTS.md" || true)" -eq 1

# ==========================================
# TEST CASE 2: 다중 에이전트 설치 (Codex + Claude)
# ==========================================
MULTI_DIR="${WORK_DIR}/MultiCase"
MULTI_PROJ="${MULTI_DIR}/Project"
CODEX_HOME_MULTI="${MULTI_DIR}/.codex"
CLAUDE_HOME_MULTI="${MULTI_DIR}/.claude"
mkdir -p "${MULTI_PROJ}" "${CODEX_HOME_MULTI}" "${CLAUDE_HOME_MULTI}"
cp "${REPO_ROOT}/sample/UECommandForgeSample.uproject" "${MULTI_PROJ}/UECommandForgeSample.uproject"

cat > "${CODEX_HOME_MULTI}/AGENTS.md" <<'AGENTS'
# Codex 지침
AGENTS
cat > "${CLAUDE_HOME_MULTI}/CLAUDE.md" <<'CLAUDE'
# Claude 지침
CLAUDE

HOME="${MULTI_DIR}" "${REPO_ROOT}/tools/release/install_local.sh" \
  --project "${MULTI_PROJ}/UECommandForgeSample.uproject" \
  --plugin-package "${PLUGIN_ZIP}" \
  --tools-package "${TOOLS_ZIP}" \
  --codex --claude \
  --run-commandlet-check false

# 양쪽에 올바르게 설치되고 블록 주입되었는지 검증
test -f "${CODEX_HOME_MULTI}/UECommandForge/uecommandforge-installed.json"
test -f "${CLAUDE_HOME_MULTI}/UECommandForge/uecommandforge-installed.json"
grep -q 'BEGIN UECOMMANDFORGE INSTRUCTIONS' "${CODEX_HOME_MULTI}/AGENTS.md"
grep -q 'BEGIN UECOMMANDFORGE INSTRUCTIONS' "${CLAUDE_HOME_MULTI}/CLAUDE.md"

# ==========================================
# TEST CASE 3: 에이전트 플래그 누락 시 에러 검증
# ==========================================
ERR_DIR="${WORK_DIR}/ErrCase"
ERR_PROJ="${ERR_DIR}/Project"
mkdir -p "${ERR_PROJ}"
cp "${REPO_ROOT}/sample/UECommandForgeSample.uproject" "${ERR_PROJ}/UECommandForgeSample.uproject"

set +e
HOME="${ERR_DIR}" "${REPO_ROOT}/tools/release/install_local.sh" \
  --project "${ERR_PROJ}/UECommandForgeSample.uproject" \
  --plugin-package "${PLUGIN_ZIP}" \
  --tools-package "${TOOLS_ZIP}" \
  --run-commandlet-check false >/dev/null 2>&1
ERR_STATUS=$?
set -e
if [ "${ERR_STATUS}" -eq 0 ]; then
  echo "Installer should fail when no agent profile flag is provided" >&2
  exit 1
fi

# ==========================================
# TEST CASE 4: 언인스톨 테스트
# ==========================================
# TEST CASE 1 의 BaseCase를 언인스톨해본다.
HOME="${BASE_DIR}" "${REPO_ROOT}/tools/release/uninstall.sh" \
  --project "${PROJECT_FILE}" \
  --codex

test ! -e "${PROJECT_DIR}/Plugins/UECommandForge"
test -e "${CODEX_HOME}/UECommandForge/tools" # --remove-agent-tools 가 false라 남아있어야 함
test ! -e "${PROJECT_DIR}/UECommandForge/uecommandforge-project.json"

# 마커도 정상 정리되었는지 검증
! grep -q 'BEGIN UECOMMANDFORGE INSTRUCTIONS' "${CODEX_HOME}/AGENTS.md"

# 재설치 진행
HOME="${BASE_DIR}" "${REPO_ROOT}/tools/release/install_local.sh" \
  --project "${PROJECT_FILE}" \
  --plugin-package "${PLUGIN_ZIP}" \
  --tools-package "${TOOLS_ZIP}" \
  --codex \
  --run-commandlet-check false

test -f "${PROJECT_DIR}/Plugins/UECommandForge/UECommandForge.uplugin"
grep -q 'BEGIN UECOMMANDFORGE INSTRUCTIONS' "${CODEX_HOME}/AGENTS.md"

# 완전히 툴까지 삭제
HOME="${BASE_DIR}" "${REPO_ROOT}/tools/release/uninstall.sh" \
  --project "${PROJECT_FILE}" \
  --codex \
  --remove-agent-tools true \
  --remove-specs true

test ! -e "${PROJECT_DIR}/Plugins/UECommandForge"
test ! -e "${CODEX_HOME}/UECommandForge/tools"
test ! -e "${CODEX_HOME}/UECommandForge/specs"
test ! -e "${CODEX_HOME}/skills/uecommandforge"
test ! -e "${CODEX_HOME}/UECommandForge"

# ==========================================
# TEST CASE 5: 역순 마커 검사 및 설치 검증 거부 테스트
# ==========================================
REVERSED_SOURCE_PROJECT="${WORK_DIR}/ReversedSourceProject"
REVERSED_SOURCE_CODEX_DIR="${WORK_DIR}/ReversedSourceCodexDir"
REVERSED_SOURCE_CODEX="${REVERSED_SOURCE_CODEX_DIR}/.codex"
REVERSED_SOURCE_OUT="${WORK_DIR}/ReversedSourceToolsPackage"
REVERSED_SOURCE_PACKAGE="${REVERSED_SOURCE_OUT}/package"
mkdir -p "${REVERSED_SOURCE_PROJECT}" "${REVERSED_SOURCE_CODEX}" "${REVERSED_SOURCE_PACKAGE}"
cp "${REPO_ROOT}/sample/UECommandForgeSample.uproject" \
  "${REVERSED_SOURCE_PROJECT}/UECommandForgeSample.uproject"
unzip -q "${TOOLS_ZIP}" -d "${REVERSED_SOURCE_PACKAGE}"
# 마커를 역순으로 만들어 비정상 스펙 패키지 유도
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
' "${REVERSED_SOURCE_PACKAGE}/specs/agent/unreal-automation-agents.md" \
  > "${REVERSED_SOURCE_PACKAGE}/specs/agent/unreal-automation-agents.md.tmp"
mv "${REVERSED_SOURCE_PACKAGE}/specs/agent/unreal-automation-agents.md.tmp" \
  "${REVERSED_SOURCE_PACKAGE}/specs/agent/unreal-automation-agents.md"
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

set +e
HOME="${REVERSED_SOURCE_CODEX_DIR}" "${REPO_ROOT}/tools/release/install_local.sh" \
  --project "${REVERSED_SOURCE_PROJECT}/UECommandForgeSample.uproject" \
  --plugin-package "${PLUGIN_ZIP}" \
  --tools-package "${REVERSED_SOURCE_OUT}/UECommandForge-${VERSION}-Tools.zip" \
  --codex \
  --run-commandlet-check false >/dev/null 2>&1
REVERSED_STATUS=$?
set -e
if [ "${REVERSED_STATUS}" -eq 0 ]; then
  echo "reversed instruction source markers should fail before install targets are created" >&2
  exit 1
fi

# ==========================================
# TEST CASE 6: 권한 검증 및 보안 가드 검사
# ==========================================
NOWRITE_PROJECT="${WORK_DIR}/NoWriteProject"
NOWRITE_DIR="${WORK_DIR}/NoWriteDir"
NOWRITE_CODEX="${NOWRITE_DIR}/.codex"
mkdir -p "${NOWRITE_PROJECT}" "${NOWRITE_CODEX}"
cp "${REPO_ROOT}/sample/UECommandForgeSample.uproject" "${NOWRITE_PROJECT}/UECommandForgeSample.uproject"
printf '# writable agents in non-writable dir\n' > "${NOWRITE_CODEX}/AGENTS.md"
chmod 600 "${NOWRITE_CODEX}/AGENTS.md"
deny_directory_writes "${NOWRITE_CODEX}"
NOWRITE_AGENT_HOME="${NOWRITE_CODEX}"
NOWRITE_DENIED=true

set +e
HOME="${NOWRITE_DIR}" "${REPO_ROOT}/tools/release/install_local.sh" \
  --project "${NOWRITE_PROJECT}/UECommandForgeSample.uproject" \
  --plugin-package "${PLUGIN_ZIP}" \
  --tools-package "${TOOLS_ZIP}" \
  --codex \
  --run-commandlet-check false >/dev/null 2>&1
NOWRITE_STATUS=$?
set -e
restore_directory_writes "${NOWRITE_CODEX}"
NOWRITE_DENIED=false

if [ "${NOWRITE_STATUS}" -eq 0 ]; then
  echo "non-writable Codex home should fail before install targets are created" >&2
  exit 1
fi

echo "ALL INSTALLER SMOKE TESTS PASSED!"
