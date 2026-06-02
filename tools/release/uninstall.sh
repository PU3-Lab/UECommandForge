#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/common.sh"

PROJECT_FILE="${UECF_PROJECT:-${PROJECT_FILE:-}}"
CODEX_HOME="${CODEX_HOME:-${HOME}/.codex}"
REMOVE_CODEX_TOOLS=false
REMOVE_SPECS=false

while [ $# -gt 0 ]; do
  case "$1" in
    --project)
      PROJECT_FILE="$2"
      shift 2
      ;;
    --codex-home)
      CODEX_HOME="$2"
      shift 2
      ;;
    --remove-codex-tools)
      REMOVE_CODEX_TOOLS="$2"
      shift 2
      ;;
    --remove-specs)
      REMOVE_SPECS="$2"
      shift 2
      ;;
    *)
      echo "[uninstall] Unknown argument: $1" >&2
      exit 2
      ;;
  esac
done

case "${REMOVE_CODEX_TOOLS}" in true|false) ;; *) echo "[uninstall] --remove-codex-tools must be true or false" >&2; exit 2 ;; esac
case "${REMOVE_SPECS}" in true|false) ;; *) echo "[uninstall] --remove-specs must be true or false" >&2; exit 2 ;; esac

if [ -z "${PROJECT_FILE}" ] || [ ! -f "${PROJECT_FILE}" ]; then
  echo "[uninstall] project file not found: ${PROJECT_FILE}" >&2
  exit 2
fi

uecf_reject_link_ancestors "${PROJECT_FILE}" "uninstall"
uecf_reject_link_ancestors "$(dirname "${PROJECT_FILE}")" "uninstall"
uecf_reject_link_ancestors "${CODEX_HOME}" "uninstall"
PROJECT_FILE="$(cd "$(dirname "${PROJECT_FILE}")" && pwd)/$(basename "${PROJECT_FILE}")"
PROJECT_DIR="$(cd "$(dirname "${PROJECT_FILE}")" && pwd)"
CODEX_HOME="$(mkdir -p "${CODEX_HOME}" && cd "${CODEX_HOME}" && pwd)"
INSTALL_ROOT="${CODEX_HOME}/UECommandForge"
LOG_DIR="${PROJECT_DIR}/Saved/UECommandForge"
LOG_FILE="${LOG_DIR}/uninstall.log"
STAMP="$(date -u +%Y%m%dT%H%M%SZ)"

reject_symlink_path() {
  uecf_reject_link_path "$1" "uninstall"
}

PROJECT_LINK_DIR="${PROJECT_DIR}/UECommandForge"
PLUGIN_DIR="${PROJECT_DIR}/Plugins/UECommandForge"
PROJECT_MANIFEST="${PROJECT_LINK_DIR}/uecommandforge-project.json"
INSTALLED_MANIFEST="${INSTALL_ROOT}/uecommandforge-installed.json"

reject_symlink_path "${PROJECT_DIR}/Plugins"
reject_symlink_path "${PLUGIN_DIR}"
reject_symlink_path "${PROJECT_LINK_DIR}"
reject_symlink_path "${PROJECT_DIR}/Saved"
reject_symlink_path "${LOG_DIR}"
reject_symlink_path "${INSTALL_ROOT}"
reject_symlink_path "${INSTALL_ROOT}/tools"
reject_symlink_path "${INSTALL_ROOT}/specs"
uecf_reject_link_ancestors "${PLUGIN_DIR}" "uninstall"
uecf_reject_link_ancestors "${INSTALL_ROOT}/tools" "uninstall"
uecf_reject_link_ancestors "${INSTALL_ROOT}/specs" "uninstall"
uecf_reject_link_tree "${PLUGIN_DIR}" "uninstall"
uecf_reject_link_tree "${INSTALL_ROOT}/tools" "uninstall"
uecf_reject_link_tree "${INSTALL_ROOT}/specs" "uninstall"
uecf_reject_output_file_path "${PROJECT_MANIFEST}" "uninstall"
uecf_reject_output_file_path "${INSTALLED_MANIFEST}" "uninstall"
uecf_reject_output_file_path "${INSTALL_ROOT}/uecommandforge.env" "uninstall"
uecf_reject_output_file_path "${INSTALL_ROOT}/uecommandforge.env.sh" "uninstall"
uecf_reject_output_file_path "${LOG_FILE}" "uninstall"

if [ ! -f "${PROJECT_MANIFEST}" ] || [ ! -f "${INSTALLED_MANIFEST}" ]; then
  echo "[uninstall] managed install metadata not found; refusing to uninstall" >&2
  exit 2
fi

jq -e \
  --arg project_file "${PROJECT_FILE}" \
  --arg plugin_path "${PLUGIN_DIR}" \
  '.project_file == $project_file and .plugin_path == $plugin_path' \
  "${PROJECT_MANIFEST}" >/dev/null
jq -e \
  --arg project_file "${PROJECT_FILE}" \
  --arg plugin_path "${PLUGIN_DIR}" \
  --arg tools_path "${INSTALL_ROOT}/tools" \
  --arg specs_path "${INSTALL_ROOT}/specs" \
  '.project_file == $project_file
   and .plugin_path == $plugin_path
   and .tools_path == $tools_path
   and .specs_path == $specs_path' \
  "${INSTALLED_MANIFEST}" >/dev/null

mkdir -p "${LOG_DIR}"
uecf_reject_link_tree "${PLUGIN_DIR}" "uninstall"
uecf_reject_output_file_path "${PROJECT_MANIFEST}" "uninstall"

rm -rf "${PLUGIN_DIR}"
rm -f "${PROJECT_MANIFEST}"
rmdir "${PROJECT_LINK_DIR}" 2>/dev/null || true

if [ "${REMOVE_CODEX_TOOLS}" = true ]; then
  uecf_reject_link_tree "${INSTALL_ROOT}/tools" "uninstall"
  rm -rf "${INSTALL_ROOT}/tools"
fi
if [ "${REMOVE_SPECS}" = true ]; then
  uecf_reject_link_tree "${INSTALL_ROOT}/specs" "uninstall"
  rm -rf "${INSTALL_ROOT}/specs"
fi
if [ "${REMOVE_CODEX_TOOLS}" = true ] && [ "${REMOVE_SPECS}" = true ]; then
  uecf_reject_output_file_path "${INSTALL_ROOT}/uecommandforge.env" "uninstall"
  uecf_reject_output_file_path "${INSTALL_ROOT}/uecommandforge.env.sh" "uninstall"
  uecf_reject_output_file_path "${INSTALLED_MANIFEST}" "uninstall"
  rm -f "${INSTALL_ROOT}/uecommandforge.env" \
    "${INSTALL_ROOT}/uecommandforge.env.sh" \
    "${INSTALLED_MANIFEST}"
fi

uecf_reject_output_file_path "${LOG_FILE}" "uninstall"
{
  echo "uninstalled_at=${STAMP}"
  echo "project_file=${PROJECT_FILE}"
  echo "removed_codex_tools=${REMOVE_CODEX_TOOLS}"
  echo "removed_specs=${REMOVE_SPECS}"
} | uecf_append_file_from_stdin "${LOG_FILE}" "uninstall"

echo "${LOG_FILE}"
