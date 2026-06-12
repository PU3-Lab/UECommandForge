#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="${SCRIPT_DIR}"
PROJECT_FILE="${UECF_PROJECT:-${PROJECT_FILE:-}}"
PLUGIN_PACKAGE=""
TOOLS_PACKAGE=""
INSTALL_ARGS=()

usage() {
  cat >&2 <<'USAGE'
Usage:
  Windows Command Prompt:
    install-uecommandforge.bat --project "C:\Path\To\MyProject\MyProject.uproject"

  PowerShell:
    .\install-uecommandforge.ps1 --project "C:\Path\To\MyProject\MyProject.uproject"

  Git Bash, macOS, or Linux:
    ./install-uecommandforge.sh --project "/path/to/MyProject.uproject" --codex

Required:
  --project <path.uproject|project-dir>
      Unreal project file, or a project folder containing exactly one .uproject file.

Optional:
  --plugin-package <zip> --tools-package <zip>
      Use existing release packages. Provide both options together.
      If omitted, the installer creates local plugin/tools packages first.

  --codex | --claude | --antigravity
      설치 대상 AI 코딩 에이전트(전역 폴더). 하나 이상 지정한다.
      --codex -> ~/.codex/AGENTS.md, --claude -> ~/.claude/CLAUDE.md,
      --antigravity -> ~/.gemini/GEMINI.md

Notes:
  Set UECF_INSTALL_SKIP_PLUGIN_BUILD=1 to reuse sample/Saved/PluginBuild.
  The install log is created after project validation at:
    <Project>/Saved/UECommandForge/install.log
USAGE
}

fail_with_usage() {
  local message="$1"
  echo "[install-uecommandforge] ERROR: ${message}" >&2
  echo >&2
  usage
}

require_value() {
  local option="$1"
  local value="${2:-}"
  if [ -z "${value}" ]; then
    fail_with_usage "Option ${option} needs a value."
    exit 2
  fi
}

while [ $# -gt 0 ]; do
  case "$1" in
    --project)
      require_value "$1" "${2:-}"
      PROJECT_FILE="$2"
      INSTALL_ARGS+=("$1" "$2")
      shift 2
      ;;
    --plugin-package|--local-package)
      require_value "$1" "${2:-}"
      PLUGIN_PACKAGE="$2"
      INSTALL_ARGS+=("--plugin-package" "$2")
      shift 2
      ;;
    --tools-package)
      require_value "$1" "${2:-}"
      TOOLS_PACKAGE="$2"
      INSTALL_ARGS+=("$1" "$2")
      shift 2
      ;;
    --backup|--run-commandlet-check)
      require_value "$1" "${2:-}"
      INSTALL_ARGS+=("$1" "$2")
      shift 2
      ;;
    --codex|--claude|--antigravity)
      INSTALL_ARGS+=("$1")
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      INSTALL_ARGS+=("$1")
      shift
      ;;
  esac
done

if [ -z "${PROJECT_FILE}" ]; then
  fail_with_usage "Missing required option: --project. Pass your Unreal project .uproject file or the project folder."
  exit 2
fi

normalize_project_file() {
  local input="$1"
  local normalized="${input}"

  if command -v cygpath >/dev/null 2>&1; then
    case "${input}" in
      [A-Za-z]:\\*|[A-Za-z]:/*)
        normalized="$(cygpath -u "${input}" 2>/dev/null || printf '%s' "${input}")"
        ;;
      *)
        if [ ! -e "${normalized}" ]; then
          normalized="$(cygpath -u "${input}" 2>/dev/null || printf '%s' "${input}")"
        fi
        ;;
    esac
  fi

  if [ -d "${normalized}" ]; then
    local matches=()
    while IFS= read -r match; do
      matches+=("${match}")
    done < <(find "${normalized}" -maxdepth 1 -type f -name '*.uproject' | sort)

    if [ "${#matches[@]}" -eq 1 ]; then
      echo "[install-uecommandforge] resolved project directory to ${matches[0]}"
      normalized="${matches[0]}"
    elif [ "${#matches[@]}" -eq 0 ]; then
      echo "[install-uecommandforge] ERROR: The --project folder does not contain a .uproject file: ${input}" >&2
      echo "[install-uecommandforge] Pass the exact .uproject file, for example:" >&2
      echo "[install-uecommandforge]   install-uecommandforge.bat --project \"C:\\Path\\To\\MyProject\\MyProject.uproject\"" >&2
      exit 2
    else
      echo "[install-uecommandforge] ERROR: The --project folder contains multiple .uproject files: ${input}" >&2
      echo "[install-uecommandforge] Pass the exact .uproject file so the installer knows which project to modify." >&2
      exit 2
    fi
  fi

  if [ ! -f "${normalized}" ]; then
    echo "[install-uecommandforge] ERROR: Could not find the project path passed to --project: ${input}" >&2
    echo "[install-uecommandforge] Check the path and include the .uproject file name, for example:" >&2
    echo "[install-uecommandforge]   install-uecommandforge.bat --project \"C:\\Path\\To\\MyProject\\MyProject.uproject\"" >&2
    exit 2
  fi

  case "${normalized}" in
    *.uproject) ;;
    *)
      echo "[install-uecommandforge] ERROR: The --project path is not a .uproject file: ${input}" >&2
      echo "[install-uecommandforge] Pass a .uproject file or a folder containing exactly one .uproject file." >&2
      exit 2
      ;;
  esac

  PROJECT_FILE="${normalized}"
}

normalize_project_file "${PROJECT_FILE}"
for index in "${!INSTALL_ARGS[@]}"; do
  if [ "${INSTALL_ARGS[$index]}" = "--project" ]; then
    INSTALL_ARGS[$((index + 1))]="${PROJECT_FILE}"
    break
  fi
done

require_windows_build_prerequisites() {
  case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) ;;
    *) return 0 ;;
  esac

  if [ "${UECF_INSTALL_SKIP_PLUGIN_BUILD:-0}" = "1" ]; then
    return 0
  fi

  if [ -d "/c/Program Files (x86)/Windows Kits/NETFXSDK" ]; then
    return 0
  fi

  local bootstrap_bat
  bootstrap_bat="$(cygpath -w "${REPO_ROOT}/tools/windows/bootstrap_dependencies.bat")"
  echo "[install-uecommandforge] Windows NetFxSDK not found; running dependency bootstrap..."
  cmd.exe //c call "${bootstrap_bat}" ue-build

  if [ -d "/c/Program Files (x86)/Windows Kits/NETFXSDK" ]; then
    return 0
  fi

  echo "[install-uecommandforge] Windows NetFxSDK is still missing after bootstrap." >&2
  echo "[install-uecommandforge] Expected directory: C:\\Program Files (x86)\\Windows Kits\\NETFXSDK" >&2
  exit 2
}

if { [ -n "${PLUGIN_PACKAGE}" ] && [ -z "${TOOLS_PACKAGE}" ]; } \
  || { [ -z "${PLUGIN_PACKAGE}" ] && [ -n "${TOOLS_PACKAGE}" ]; }; then
  fail_with_usage "Package install needs both --plugin-package and --tools-package. Omit both options to let the installer create packages automatically."
  exit 2
fi

if [ -z "${PLUGIN_PACKAGE}" ] && [ -z "${TOOLS_PACKAGE}" ]; then
  require_windows_build_prerequisites

  VERSION="$(jq -r '.VersionName' "${REPO_ROOT}/sample/Plugins/UECommandForge/UECommandForge.uplugin")"
  CHANNEL="${UECF_INSTALL_CHANNEL:-local}"
  OUT_ROOT="${UECF_INSTALL_PACKAGE_DIR:-${REPO_ROOT}/sample/Saved/Release/Installer}"
  PLATFORM="Mac"
  case "$(uname -s)" in
    Darwin) PLATFORM="Mac" ;;
    Linux) PLATFORM="Linux" ;;
    MINGW*|MSYS*|CYGWIN*) PLATFORM="Win64" ;;
  esac

  PLUGIN_OUT="${OUT_ROOT}/Plugin"
  TOOLS_OUT="${OUT_ROOT}/Tools"
  mkdir -p "${PLUGIN_OUT}" "${TOOLS_OUT}"
  PACKAGE_LOG_DIR="${OUT_ROOT}/Logs"
  mkdir -p "${PACKAGE_LOG_DIR}"

  PACKAGE_PLUGIN_ARGS=(
    --version "${VERSION}"
    --channel "${CHANNEL}"
    --out-dir "${PLUGIN_OUT}"
  )
  if [ "${UECF_INSTALL_SKIP_PLUGIN_BUILD:-0}" = "1" ]; then
    PACKAGE_PLUGIN_ARGS+=(--skip-build)
  fi

  echo "[install-uecommandforge] packaging plugin for ${PLATFORM}..."
  PLUGIN_PACKAGE_LOG="${PACKAGE_LOG_DIR}/package-plugin.log"
  "${REPO_ROOT}/tools/release/package_plugin.sh" "${PACKAGE_PLUGIN_ARGS[@]}" \
    2>&1 | tee "${PLUGIN_PACKAGE_LOG}"
  PLUGIN_PACKAGE="$(tail -n 1 "${PLUGIN_PACKAGE_LOG}" | tr -d '\r')"

  echo "[install-uecommandforge] packaging Codex tools..."
  TOOLS_PACKAGE_LOG="${PACKAGE_LOG_DIR}/package-tools.log"
  "${REPO_ROOT}/tools/release/package_tools.sh" \
    --version "${VERSION}" \
    --channel "${CHANNEL}" \
    --out-dir "${TOOLS_OUT}" \
    2>&1 | tee "${TOOLS_PACKAGE_LOG}"
  TOOLS_PACKAGE="$(tail -n 1 "${TOOLS_PACKAGE_LOG}" | tr -d '\r')"

  if [ ! -f "${PLUGIN_PACKAGE}" ]; then
    PLUGIN_PACKAGE="${PLUGIN_OUT}/UECommandForge-${VERSION}-UE5.7-${PLATFORM}.zip"
  fi
  if [ ! -f "${TOOLS_PACKAGE}" ]; then
    TOOLS_PACKAGE="${TOOLS_OUT}/UECommandForge-${VERSION}-Tools.zip"
  fi

  INSTALL_ARGS+=(--plugin-package "${PLUGIN_PACKAGE}" --tools-package "${TOOLS_PACKAGE}")
fi

echo "[install-uecommandforge] installing into ${PROJECT_FILE}..."
exec "${REPO_ROOT}/tools/release/install_local.sh" "${INSTALL_ARGS[@]}"
