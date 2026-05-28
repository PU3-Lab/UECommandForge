#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_FILE="${UECF_PROJECT:-${PROJECT_FILE:-}}"
PLUGIN_PACKAGE=""
TOOLS_PACKAGE=""
CODEX_HOME="${CODEX_HOME:-${HOME}/.codex}"
BACKUP=true
RUN_COMMANDLET_CHECK=false

while [ $# -gt 0 ]; do
  case "$1" in
    --project)
      PROJECT_FILE="$2"
      shift 2
      ;;
    --plugin-package|--local-package)
      PLUGIN_PACKAGE="$2"
      shift 2
      ;;
    --tools-package)
      TOOLS_PACKAGE="$2"
      shift 2
      ;;
    --codex-home)
      CODEX_HOME="$2"
      shift 2
      ;;
    --backup)
      BACKUP="$2"
      shift 2
      ;;
    --run-commandlet-check)
      RUN_COMMANDLET_CHECK="$2"
      shift 2
      ;;
    *)
      echo "[install_local] Unknown argument: $1" >&2
      exit 2
      ;;
  esac
done

case "${BACKUP}" in true|false) ;; *) echo "[install_local] --backup must be true or false" >&2; exit 2 ;; esac
case "${RUN_COMMANDLET_CHECK}" in true|false) ;; *) echo "[install_local] --run-commandlet-check must be true or false" >&2; exit 2 ;; esac

if [ -z "${PROJECT_FILE}" ] || [ -z "${PLUGIN_PACKAGE}" ] || [ -z "${TOOLS_PACKAGE}" ]; then
  echo "Usage: $0 --project <path.uproject> --plugin-package <zip> --tools-package <zip> [--codex-home <path>]" >&2
  exit 2
fi

if [ ! -f "${PROJECT_FILE}" ]; then
  echo "[install_local] project file not found: ${PROJECT_FILE}" >&2
  exit 2
fi
if [ ! -f "${PLUGIN_PACKAGE}" ]; then
  echo "[install_local] plugin package not found: ${PLUGIN_PACKAGE}" >&2
  exit 2
fi
if [ ! -f "${TOOLS_PACKAGE}" ]; then
  echo "[install_local] tools package not found: ${TOOLS_PACKAGE}" >&2
  exit 2
fi

PROJECT_FILE="$(cd "$(dirname "${PROJECT_FILE}")" && pwd)/$(basename "${PROJECT_FILE}")"
PROJECT_DIR="$(cd "$(dirname "${PROJECT_FILE}")" && pwd)"
CODEX_HOME="$(mkdir -p "${CODEX_HOME}" && cd "${CODEX_HOME}" && pwd)"
INSTALL_ROOT="${CODEX_HOME}/UECommandForge"
PLUGIN_DIR="${PROJECT_DIR}/Plugins/UECommandForge"
PROJECT_LINK_DIR="${PROJECT_DIR}/UECommandForge"
LOG_DIR="${PROJECT_DIR}/Saved/UECommandForge"
LOG_FILE="${LOG_DIR}/install.log"
STAMP="$(date -u +%Y%m%dT%H%M%SZ)-$$"
BACKUP_ROOT="${PROJECT_DIR}/Saved/UECommandForge/Backups/${STAMP}"

reject_symlink_path() {
  local path="$1"
  if [ -L "${path}" ]; then
    echo "[install_local] refusing to operate on symlink path: ${path}" >&2
    exit 2
  fi
}

require_managed_existing_targets() {
  local project_manifest="${PROJECT_LINK_DIR}/uecommandforge-project.json"
  local installed_manifest="${INSTALL_ROOT}/uecommandforge-installed.json"
  local has_project_state=false
  local has_codex_state=false

  if [ -e "${PLUGIN_DIR}" ] || [ -e "${PROJECT_LINK_DIR}" ] || [ -e "${project_manifest}" ]; then
    has_project_state=true
  fi
  if [ -e "${INSTALL_ROOT}/tools" ] || [ -e "${INSTALL_ROOT}/specs" ] \
    || [ -e "${INSTALL_ROOT}/uecommandforge.env" ] || [ -e "${installed_manifest}" ]; then
    has_codex_state=true
  fi

  if [ "${has_project_state}" = false ] && [ "${has_codex_state}" = false ]; then
    return 0
  fi

  if [ "${has_project_state}" = true ]; then
    if [ ! -f "${project_manifest}" ]; then
      echo "[install_local] existing project install targets are unmanaged; refusing to overwrite" >&2
      exit 2
    fi
    jq -e \
      --arg project_file "${PROJECT_FILE}" \
      --arg plugin_path "${PLUGIN_DIR}" \
      '.project_file == $project_file and .plugin_path == $plugin_path' \
      "${project_manifest}" >/dev/null
  fi

  if [ "${has_codex_state}" = true ]; then
    if [ ! -f "${installed_manifest}" ]; then
      echo "[install_local] existing Codex install targets are unmanaged; refusing to overwrite" >&2
      exit 2
    fi
    jq -e \
      --arg project_file "${PROJECT_FILE}" \
      --arg plugin_path "${PLUGIN_DIR}" \
      --arg tools_path "${INSTALL_ROOT}/tools" \
      --arg specs_path "${INSTALL_ROOT}/specs" \
      '.project_file == $project_file
       and .plugin_path == $plugin_path
       and .tools_path == $tools_path
       and .specs_path == $specs_path' \
      "${installed_manifest}" >/dev/null
  fi
}

reject_symlink_path "${PROJECT_DIR}/Plugins"
reject_symlink_path "${PROJECT_DIR}/Plugins/UECommandForge"
reject_symlink_path "${PROJECT_DIR}/UECommandForge"
reject_symlink_path "${PROJECT_DIR}/Saved"
reject_symlink_path "${PROJECT_DIR}/Saved/UECommandForge"
reject_symlink_path "${INSTALL_ROOT}"
reject_symlink_path "${INSTALL_ROOT}/tools"
reject_symlink_path "${INSTALL_ROOT}/specs"
require_managed_existing_targets

mkdir -p "${LOG_DIR}" "${INSTALL_ROOT}" "${PROJECT_LINK_DIR}"

verify_package() {
  local zip_path="$1"
  local checksum_path
  checksum_path="$(dirname "${zip_path}")/checksums.txt"
  if [ ! -f "${checksum_path}" ]; then
    echo "[install_local] checksums file not found for package: ${zip_path}" >&2
    exit 2
  fi
  if ! grep -q "$(basename "${zip_path}")" "${checksum_path}"; then
    echo "[install_local] checksums file does not reference package: ${zip_path}" >&2
    exit 2
  fi
  "${SCRIPT_DIR}/verify_release_package.sh" "${zip_path}" "${checksum_path}" >/dev/null
}

copy_tree() {
  local src="$1"
  local dest="$2"
  mkdir -p "$(dirname "${dest}")"
  rm -rf "${dest}"
  cp -R "${src}" "${dest}"
}

verify_package "${PLUGIN_PACKAGE}"
verify_package "${TOOLS_PACKAGE}"

WORK_DIR="$(mktemp -d)"
cleanup() {
  rm -rf "${WORK_DIR}"
}
trap cleanup EXIT

PLUGIN_ROOT="${WORK_DIR}/plugin"
TOOLS_ROOT="${WORK_DIR}/tools"
mkdir -p "${PLUGIN_ROOT}" "${TOOLS_ROOT}"
unzip -q "${PLUGIN_PACKAGE}" -d "${PLUGIN_ROOT}"
unzip -q "${TOOLS_PACKAGE}" -d "${TOOLS_ROOT}"

if [ ! -f "${PLUGIN_ROOT}/UECommandForge.uplugin" ]; then
  echo "[install_local] plugin package missing UECommandForge.uplugin" >&2
  exit 2
fi
if [ ! -d "${TOOLS_ROOT}/tools" ] || [ ! -d "${TOOLS_ROOT}/specs" ]; then
  echo "[install_local] tools package missing tools/specs" >&2
  exit 2
fi

VERSION="$(jq -r '.VersionName' "${PLUGIN_ROOT}/UECommandForge.uplugin")"
PLUGIN_MANIFEST="${PLUGIN_ROOT}/uecommandforge-manifest.json"
TOOLS_MANIFEST="${TOOLS_ROOT}/uecommandforge-manifest.json"

jq -e \
  --arg version "${VERSION}" \
  '.version == $version' \
  "${PLUGIN_MANIFEST}" >/dev/null
jq -e \
  --arg version "${VERSION}" \
  '.version == $version' \
  "${TOOLS_MANIFEST}" >/dev/null
jq -e \
  --slurpfile plugin_manifest "${PLUGIN_MANIFEST}" \
  '.release_channel == $plugin_manifest[0].release_channel' \
  "${TOOLS_MANIFEST}" >/dev/null

if [ "${BACKUP}" = true ]; then
  mkdir -p "${BACKUP_ROOT}"
  if [ -d "${PLUGIN_DIR}" ]; then
    mkdir -p "${BACKUP_ROOT}/Plugins"
    mv "${PLUGIN_DIR}" "${BACKUP_ROOT}/Plugins/UECommandForge"
  fi
  if [ -d "${INSTALL_ROOT}/tools" ]; then
    mkdir -p "${BACKUP_ROOT}/Codex"
    mv "${INSTALL_ROOT}/tools" "${BACKUP_ROOT}/Codex/tools"
  fi
  if [ -d "${INSTALL_ROOT}/specs" ]; then
    mkdir -p "${BACKUP_ROOT}/Codex"
    mv "${INSTALL_ROOT}/specs" "${BACKUP_ROOT}/Codex/specs"
  fi
else
  rm -rf "${PLUGIN_DIR}" "${INSTALL_ROOT}/tools" "${INSTALL_ROOT}/specs"
fi

mkdir -p "$(dirname "${PLUGIN_DIR}")"
cp -R "${PLUGIN_ROOT}" "${PLUGIN_DIR}"
rm -f "${PLUGIN_DIR}/uecommandforge-manifest.json" \
  "${PLUGIN_DIR}/checksums.txt" \
  "${PLUGIN_DIR}/install.md" \
  "${PLUGIN_DIR}/release-notes.md" \
  "${PLUGIN_DIR}/validation-report.json"

copy_tree "${TOOLS_ROOT}/tools" "${INSTALL_ROOT}/tools"
copy_tree "${TOOLS_ROOT}/specs" "${INSTALL_ROOT}/specs"
find "${INSTALL_ROOT}/tools" -type f -name '*.sh' -exec chmod +x {} +

cat > "${INSTALL_ROOT}/uecommandforge.env" <<ENV
UECF_PROJECT_FILE=${PROJECT_FILE}
CODEX_HOME=${CODEX_HOME}
UECF_INSTALL_ROOT=${INSTALL_ROOT}
ENV

jq -n \
  --arg installed_at "${STAMP}" \
  --arg version "${VERSION}" \
  --arg project_file "${PROJECT_FILE}" \
  --arg plugin_path "${PLUGIN_DIR}" \
  --arg tools_path "${INSTALL_ROOT}/tools" \
  --arg specs_path "${INSTALL_ROOT}/specs" \
  --arg codex_home "${CODEX_HOME}" \
  --arg source_release "${PLUGIN_PACKAGE}" \
  --arg backup_path "${BACKUP_ROOT}" \
  --slurpfile plugin_manifest "${PLUGIN_MANIFEST}" \
  --slurpfile tools_manifest "${TOOLS_MANIFEST}" \
  '{
    installed_at: $installed_at,
    version: $version,
    project_file: $project_file,
    plugin_path: $plugin_path,
    tools_path: $tools_path,
    specs_path: $specs_path,
    codex_home: $codex_home,
    source_release: $source_release,
    checksums: {
      plugin: $plugin_manifest[0].checksums,
      tools: $tools_manifest[0].checksums
    },
    backup_path: $backup_path,
    last_validation_report: null
  }' > "${INSTALL_ROOT}/uecommandforge-installed.json"

jq -n \
  --arg project_file "${PROJECT_FILE}" \
  --arg plugin_path "${PLUGIN_DIR}" \
  --arg codex_home "${CODEX_HOME}" \
  --arg codex_tools_path "${INSTALL_ROOT}/tools" \
  --arg codex_specs_path "${INSTALL_ROOT}/specs" \
  --arg installed_version "${VERSION}" \
  --arg installed_manifest_path "${INSTALL_ROOT}/uecommandforge-installed.json" \
  --arg default_env_path "${INSTALL_ROOT}/uecommandforge.env" \
  '{
    project_file: $project_file,
    plugin_path: $plugin_path,
    codex_home: $codex_home,
    codex_tools_path: $codex_tools_path,
    codex_specs_path: $codex_specs_path,
    installed_version: $installed_version,
    installed_manifest_path: $installed_manifest_path,
    default_env_path: $default_env_path
  }' > "${PROJECT_LINK_DIR}/uecommandforge-project.json"

{
  echo "installed_at=${STAMP}"
  echo "version=${VERSION}"
  echo "project_file=${PROJECT_FILE}"
  echo "plugin_path=${PLUGIN_DIR}"
  echo "tools_path=${INSTALL_ROOT}/tools"
  echo "specs_path=${INSTALL_ROOT}/specs"
} >> "${LOG_FILE}"

if [ "${RUN_COMMANDLET_CHECK}" = true ]; then
  UECF_PROJECT_FILE="${PROJECT_FILE}" "${INSTALL_ROOT}/tools/ue/hello.sh" >/dev/null
fi

echo "${INSTALL_ROOT}/uecommandforge-installed.json"
