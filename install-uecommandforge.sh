#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="${SCRIPT_DIR}"
PROJECT_FILE="${UECF_PROJECT:-${PROJECT_FILE:-}}"
PLUGIN_PACKAGE=""
TOOLS_PACKAGE=""
INSTALL_ARGS=()

usage() {
  cat >&2 <<USAGE
Usage: $0 --project <path.uproject> [--plugin-package <zip> --tools-package <zip>] [--codex-home <path>]

If both package paths are omitted, this wrapper creates local plugin/tools packages first.
Set UECF_INSTALL_SKIP_PLUGIN_BUILD=1 to reuse an existing sample/Saved/PluginBuild output.
USAGE
}

require_value() {
  local option="$1"
  local value="${2:-}"
  if [ -z "${value}" ]; then
    echo "[install-uecommandforge] ${option} requires a value" >&2
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
    --codex-home|--backup|--run-commandlet-check)
      require_value "$1" "${2:-}"
      INSTALL_ARGS+=("$1" "$2")
      shift 2
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
  usage
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
    else
      echo "[install-uecommandforge] --project must point to a .uproject file, or a directory with exactly one .uproject: ${input}" >&2
      exit 2
    fi
  fi

  if [ ! -f "${normalized}" ]; then
    echo "[install-uecommandforge] project file not found: ${input}" >&2
    exit 2
  fi

  case "${normalized}" in
    *.uproject) ;;
    *)
      echo "[install-uecommandforge] --project must point to a .uproject file: ${input}" >&2
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
  echo "[install-uecommandforge] provide both --plugin-package and --tools-package, or omit both for auto packaging" >&2
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
