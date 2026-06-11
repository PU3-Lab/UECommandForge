#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/common.sh"
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

uecf_reject_link_ancestors "${CODEX_HOME}" "install_local"
uecf_reject_link_ancestors "${PROJECT_FILE}" "install_local"
uecf_reject_link_ancestors "$(dirname "${PROJECT_FILE}")" "install_local"
PROJECT_FILE="$(cd "$(dirname "${PROJECT_FILE}")" && pwd)/$(basename "${PROJECT_FILE}")"
PROJECT_DIR="$(cd "$(dirname "${PROJECT_FILE}")" && pwd)"
CODEX_HOME="$(mkdir -p "${CODEX_HOME}" && cd "${CODEX_HOME}" && pwd -P)"
INSTALL_ROOT="${CODEX_HOME}/UECommandForge"
CODEX_SKILL_DIR="${CODEX_HOME}/skills/uecommandforge"
PLUGIN_DIR="${PROJECT_DIR}/Plugins/UECommandForge"
PROJECT_LINK_DIR="${PROJECT_DIR}/UECommandForge"
LOG_DIR="${PROJECT_DIR}/Saved/UECommandForge"
LOG_FILE="${LOG_DIR}/install.log"
CODEX_AGENTS_FILE="${CODEX_HOME}/AGENTS.md"
CODEX_ENV_FILE="${INSTALL_ROOT}/uecommandforge.env"
CODEX_SHELL_ENV_FILE="${INSTALL_ROOT}/uecommandforge.env.sh"
INSTALLED_MANIFEST="${INSTALL_ROOT}/uecommandforge-installed.json"
PROJECT_MANIFEST="${PROJECT_LINK_DIR}/uecommandforge-project.json"
STAMP="$(date -u +%Y%m%dT%H%M%SZ)-$$"
BACKUP_ROOT="${PROJECT_DIR}/Saved/UECommandForge/Backups/${STAMP}"

reject_symlink_path() {
  uecf_reject_link_path "$1" "install_local"
}

require_managed_existing_targets() {
  local has_project_state=false
  local has_codex_state=false

  if [ -e "${PLUGIN_DIR}" ] || [ -e "${PROJECT_LINK_DIR}" ] || [ -e "${PROJECT_MANIFEST}" ]; then
    has_project_state=true
  fi
  if [ -e "${INSTALL_ROOT}/tools" ] || [ -e "${INSTALL_ROOT}/specs" ] \
    || [ -e "${CODEX_SKILL_DIR}" ] \
    || [ -e "${CODEX_ENV_FILE}" ] || [ -e "${CODEX_SHELL_ENV_FILE}" ] \
    || [ -e "${INSTALLED_MANIFEST}" ]; then
    has_codex_state=true
  fi

  if [ "${has_project_state}" = false ] && [ "${has_codex_state}" = false ]; then
    return 0
  fi

  if [ "${has_project_state}" = true ]; then
    if [ ! -f "${PROJECT_MANIFEST}" ]; then
      echo "[install_local] existing project install targets are unmanaged; refusing to overwrite" >&2
      exit 2
    fi
    jq -e \
      --arg project_file "${PROJECT_FILE}" \
      --arg plugin_path "${PLUGIN_DIR}" \
      '.project_file == $project_file and .plugin_path == $plugin_path' \
      "${PROJECT_MANIFEST}" >/dev/null
  fi

  if [ "${has_codex_state}" = true ]; then
    if [ ! -f "${INSTALLED_MANIFEST}" ]; then
      echo "[install_local] existing Codex install targets are unmanaged; refusing to overwrite" >&2
      exit 2
    fi
    jq -e \
      --arg project_file "${PROJECT_FILE}" \
      --arg plugin_path "${PLUGIN_DIR}" \
      --arg tools_path "${INSTALL_ROOT}/tools" \
      --arg specs_path "${INSTALL_ROOT}/specs" \
      --arg skill_path "${CODEX_SKILL_DIR}" \
      '.project_file == $project_file
       and .plugin_path == $plugin_path
       and .tools_path == $tools_path
       and .specs_path == $specs_path
       and ((.skill_path // $skill_path) == $skill_path)' \
      "${INSTALLED_MANIFEST}" >/dev/null
    if [ -e "${CODEX_SKILL_DIR}" ]; then
      jq -e \
        --arg skill_path "${CODEX_SKILL_DIR}" \
        '.skill_path == $skill_path' \
        "${INSTALLED_MANIFEST}" >/dev/null
    fi
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
reject_symlink_path "${CODEX_HOME}/skills"
reject_symlink_path "${CODEX_SKILL_DIR}"
reject_symlink_path "${CODEX_AGENTS_FILE}"
uecf_reject_output_file_path "${CODEX_ENV_FILE}" "install_local"
uecf_reject_output_file_path "${CODEX_SHELL_ENV_FILE}" "install_local"
uecf_reject_output_file_path "${INSTALLED_MANIFEST}" "install_local"
uecf_reject_output_file_path "${PROJECT_MANIFEST}" "install_local"
uecf_reject_output_file_path "${LOG_FILE}" "install_local"
uecf_reject_link_tree "${PLUGIN_DIR}" "install_local"
uecf_reject_link_tree "${INSTALL_ROOT}/tools" "install_local"
uecf_reject_link_tree "${INSTALL_ROOT}/specs" "install_local"
uecf_reject_link_tree "${CODEX_SKILL_DIR}" "install_local"
require_managed_existing_targets

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

stage_package_for_install() {
  local zip_path="$1"
  local label="$2"
  local source_checksum_path
  local staged_dir
  local staged_zip_path

  source_checksum_path="$(dirname "${zip_path}")/checksums.txt"
  staged_dir="${WORK_DIR}/${label}-package"
  mkdir -p "${staged_dir}"
  staged_zip_path="${staged_dir}/$(basename "${zip_path}")"
  cp "${zip_path}" "${staged_zip_path}"
  cp "${source_checksum_path}" "${staged_dir}/checksums.txt"
  verify_package "${staged_zip_path}"
  printf '%s\n' "${staged_zip_path}"
}

copy_tree() {
  local src="$1"
  local dest="$2"
  mkdir -p "$(dirname "${dest}")"
  uecf_reject_link_tree "${dest}" "install_local"
  rm -rf "${dest}"
  cp -R "${src}" "${dest}"
}

require_ordered_marker_pair() {
  local path="$1"
  local begin_marker="$2"
  local end_marker="$3"
  local mode="$4"
  local error_message="$5"

  if ! awk \
    -v begin_marker="${begin_marker}" \
    -v end_marker="${end_marker}" \
    -v mode="${mode}" '
      index($0, begin_marker) {
        if (state != 0 || index($0, end_marker)) {
          invalid = 1
          exit 1
        }
        state = 1
        next
      }
      index($0, end_marker) {
        if (state != 1) {
          invalid = 1
          exit 1
        }
        state = 2
        next
      }
      END {
        if (invalid || (mode == "required" && state != 2) || (mode == "optional" && state != 0 && state != 2)) {
          exit 1
        }
      }
    ' "${path}"; then
    echo "${error_message}" >&2
    exit 2
  fi
}

require_instruction_source() {
  local instructions_file="$1"
  if [ ! -f "${instructions_file}" ]; then
    echo "[install_local] Codex instruction source is missing: ${instructions_file}" >&2
    exit 2
  fi
  require_ordered_marker_pair \
    "${instructions_file}" \
    "BEGIN UECOMMANDFORGE MANAGED CONTENT" \
    "END UECOMMANDFORGE MANAGED CONTENT" \
    required \
    "[install_local] Codex instruction source has invalid managed content markers: ${instructions_file}"
}

require_codex_agents_appendable() {
  if [ ! -w "${CODEX_HOME}" ]; then
    echo "[install_local] Codex home is not writable: ${CODEX_HOME}" >&2
    exit 2
  fi

  local probe_file
  probe_file="$(mktemp "${CODEX_HOME}/.AGENTS.md.preflight.XXXXXX")"
  if ! mv "${probe_file}" "${probe_file}.rename"; then
    rm -f "${probe_file}" "${probe_file}.rename"
    echo "[install_local] Codex home cannot atomically update AGENTS.md: ${CODEX_HOME}" >&2
    exit 2
  fi
  rm -f "${probe_file}.rename"

  if [ -e "${CODEX_AGENTS_FILE}" ]; then
    if [ ! -f "${CODEX_AGENTS_FILE}" ]; then
      echo "[install_local] Codex AGENTS path is not a regular file: ${CODEX_AGENTS_FILE}" >&2
      exit 2
    fi
    if [ ! -w "${CODEX_AGENTS_FILE}" ]; then
      echo "[install_local] Codex AGENTS file is not writable: ${CODEX_AGENTS_FILE}" >&2
      echo "[install_local] Existing AGENTS.md was preserved; fix file permissions and retry." >&2
      exit 2
    fi
    require_ordered_marker_pair \
      "${CODEX_AGENTS_FILE}" \
      "BEGIN UECOMMANDFORGE CODEX INSTRUCTIONS" \
      "END UECOMMANDFORGE CODEX INSTRUCTIONS" \
      optional \
      "[install_local] Existing Codex AGENTS.md has an invalid UECommandForge managed block; fix markers and retry: ${CODEX_AGENTS_FILE}"
  fi
}

append_codex_agents_instructions() {
  local temp_file
  local begin_count=0
  local instructions_file="${TOOLS_ROOT}/specs/codex/unreal-automation-agents.md"
  require_instruction_source "${instructions_file}"
  reject_symlink_path "${CODEX_AGENTS_FILE}"

  temp_file="$(mktemp "${CODEX_HOME}/.AGENTS.md.XXXXXX")"
  if [ -f "${CODEX_AGENTS_FILE}" ]; then
    cp -p "${CODEX_AGENTS_FILE}" "${temp_file}"
    require_ordered_marker_pair \
      "${CODEX_AGENTS_FILE}" \
      "BEGIN UECOMMANDFORGE CODEX INSTRUCTIONS" \
      "END UECOMMANDFORGE CODEX INSTRUCTIONS" \
      optional \
      "[install_local] Existing Codex AGENTS.md has an invalid UECommandForge managed block; fix markers and retry: ${CODEX_AGENTS_FILE}"
    begin_count="$(grep -c 'BEGIN UECOMMANDFORGE CODEX INSTRUCTIONS' "${CODEX_AGENTS_FILE}" || true)"
    if [ "${begin_count}" -eq 1 ]; then
      awk '
        /BEGIN UECOMMANDFORGE CODEX INSTRUCTIONS/ { skipping = 1; next }
        /END UECOMMANDFORGE CODEX INSTRUCTIONS/ { skipping = 0; next }
        skipping != 1 { print }
      ' "${CODEX_AGENTS_FILE}" > "${temp_file}.stripped"
      cat "${temp_file}.stripped" > "${temp_file}"
      rm -f "${temp_file}.stripped"
    fi
  fi

  if [ -s "${CODEX_AGENTS_FILE}" ]; then
    printf '\n' >> "${temp_file}"
  fi

cat >> "${temp_file}" <<'AGENTS'
<!-- BEGIN UECOMMANDFORGE CODEX INSTRUCTIONS -->
AGENTS
  awk '
    /BEGIN UECOMMANDFORGE MANAGED CONTENT/ { copying = 1; next }
    /END UECOMMANDFORGE MANAGED CONTENT/ { copying = 0; next }
    copying == 1 { print }
  ' "${instructions_file}" >> "${temp_file}"
  printf '\n' >> "${temp_file}"
cat >> "${temp_file}" <<'AGENTS'
<!-- END UECOMMANDFORGE CODEX INSTRUCTIONS -->
AGENTS

  mv "${temp_file}" "${CODEX_AGENTS_FILE}"
}

WORK_DIR="$(mktemp -d)"
cleanup() {
  rm -rf "${WORK_DIR}"
}
trap cleanup EXIT

PLUGIN_STAGED_PACKAGE="$(stage_package_for_install "${PLUGIN_PACKAGE}" "plugin")"
TOOLS_STAGED_PACKAGE="$(stage_package_for_install "${TOOLS_PACKAGE}" "tools")"

PLUGIN_ROOT="${WORK_DIR}/plugin"
TOOLS_ROOT="${WORK_DIR}/tools"
mkdir -p "${PLUGIN_ROOT}" "${TOOLS_ROOT}"
unzip -q "${PLUGIN_STAGED_PACKAGE}" -d "${PLUGIN_ROOT}"
unzip -q "${TOOLS_STAGED_PACKAGE}" -d "${TOOLS_ROOT}"
uecf_reject_link_tree "${PLUGIN_ROOT}" "install_local"
uecf_reject_link_tree "${TOOLS_ROOT}" "install_local"

if [ ! -f "${PLUGIN_ROOT}/UECommandForge.uplugin" ]; then
  echo "[install_local] plugin package missing UECommandForge.uplugin" >&2
  exit 2
fi
if [ ! -d "${TOOLS_ROOT}/tools" ] || [ ! -d "${TOOLS_ROOT}/specs" ] \
  || [ ! -d "${TOOLS_ROOT}/skills/uecommandforge" ]; then
  echo "[install_local] tools package missing tools/specs/skills" >&2
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
  '.package_type == "plugin"' \
  "${PLUGIN_MANIFEST}" >/dev/null
jq -e \
  --arg version "${VERSION}" \
  '.version == $version' \
  "${TOOLS_MANIFEST}" >/dev/null
jq -e \
  '.package_type == "tools"' \
  "${TOOLS_MANIFEST}" >/dev/null
jq -e \
  --slurpfile plugin_manifest "${PLUGIN_MANIFEST}" \
  '.release_channel == $plugin_manifest[0].release_channel' \
  "${TOOLS_MANIFEST}" >/dev/null

require_instruction_source "${TOOLS_ROOT}/specs/codex/unreal-automation-agents.md"
require_codex_agents_appendable
mkdir -p "${LOG_DIR}" "${INSTALL_ROOT}" "${PROJECT_LINK_DIR}"

if [ "${BACKUP}" = true ]; then
  uecf_reject_link_ancestors "${BACKUP_ROOT}" "install_local"
  uecf_reject_link_tree "${PROJECT_DIR}/Saved/UECommandForge/Backups" "install_local"
  uecf_reject_link_tree "${BACKUP_ROOT}" "install_local"
  mkdir -p "${BACKUP_ROOT}"
  if [ -d "${PLUGIN_DIR}" ]; then
    uecf_reject_link_tree "${PLUGIN_DIR}" "install_local"
    uecf_reject_link_ancestors "${BACKUP_ROOT}/Plugins/UECommandForge" "install_local"
    mkdir -p "${BACKUP_ROOT}/Plugins"
    mv "${PLUGIN_DIR}" "${BACKUP_ROOT}/Plugins/UECommandForge"
  fi
  if [ -d "${INSTALL_ROOT}/tools" ]; then
    uecf_reject_link_tree "${INSTALL_ROOT}/tools" "install_local"
    uecf_reject_link_ancestors "${BACKUP_ROOT}/Codex/tools" "install_local"
    mkdir -p "${BACKUP_ROOT}/Codex"
    mv "${INSTALL_ROOT}/tools" "${BACKUP_ROOT}/Codex/tools"
  fi
  if [ -d "${INSTALL_ROOT}/specs" ]; then
    uecf_reject_link_tree "${INSTALL_ROOT}/specs" "install_local"
    uecf_reject_link_ancestors "${BACKUP_ROOT}/Codex/specs" "install_local"
    mkdir -p "${BACKUP_ROOT}/Codex"
    mv "${INSTALL_ROOT}/specs" "${BACKUP_ROOT}/Codex/specs"
  fi
  if [ -d "${CODEX_SKILL_DIR}" ]; then
    uecf_reject_link_tree "${CODEX_SKILL_DIR}" "install_local"
    uecf_reject_link_ancestors "${BACKUP_ROOT}/Codex/skills/uecommandforge" "install_local"
    mkdir -p "${BACKUP_ROOT}/Codex/skills"
    mv "${CODEX_SKILL_DIR}" "${BACKUP_ROOT}/Codex/skills/uecommandforge"
  fi
else
  uecf_reject_link_tree "${PLUGIN_DIR}" "install_local"
  uecf_reject_link_tree "${INSTALL_ROOT}/tools" "install_local"
  uecf_reject_link_tree "${INSTALL_ROOT}/specs" "install_local"
  uecf_reject_link_tree "${CODEX_SKILL_DIR}" "install_local"
  rm -rf "${PLUGIN_DIR}" "${INSTALL_ROOT}/tools" "${INSTALL_ROOT}/specs" "${CODEX_SKILL_DIR}"
fi

mkdir -p "$(dirname "${PLUGIN_DIR}")"
uecf_reject_link_tree "${PLUGIN_DIR}" "install_local"
cp -R "${PLUGIN_ROOT}" "${PLUGIN_DIR}"
rm -f "${PLUGIN_DIR}/uecommandforge-manifest.json" \
  "${PLUGIN_DIR}/checksums.txt" \
  "${PLUGIN_DIR}/install.md" \
  "${PLUGIN_DIR}/release-notes.md" \
  "${PLUGIN_DIR}/validation-report.json"

copy_tree "${TOOLS_ROOT}/tools" "${INSTALL_ROOT}/tools"
copy_tree "${TOOLS_ROOT}/specs" "${INSTALL_ROOT}/specs"
copy_tree "${TOOLS_ROOT}/skills/uecommandforge" "${CODEX_SKILL_DIR}"
find "${INSTALL_ROOT}/tools" -type f -name '*.sh' -exec chmod +x {} +
append_codex_agents_instructions

uecf_reject_output_file_path "${CODEX_ENV_FILE}" "install_local"
uecf_write_file_from_stdin "${CODEX_ENV_FILE}" "install_local" <<ENV
UECF_PROJECT_FILE=${PROJECT_FILE}
CODEX_HOME=${CODEX_HOME}
UECF_INSTALL_ROOT=${INSTALL_ROOT}
ENV

uecf_reject_output_file_path "${CODEX_SHELL_ENV_FILE}" "install_local"
uecf_write_file_from_stdin "${CODEX_SHELL_ENV_FILE}" "install_local" <<ENV
export UECF_PROJECT_FILE=$(uecf_shell_quote "${PROJECT_FILE}")
export CODEX_HOME=$(uecf_shell_quote "${CODEX_HOME}")
export UECF_INSTALL_ROOT=$(uecf_shell_quote "${INSTALL_ROOT}")
ENV

uecf_reject_output_file_path "${INSTALLED_MANIFEST}" "install_local"
installed_manifest_temp="$(uecf_prepare_output_temp_file "${INSTALLED_MANIFEST}" "install_local")"
if ! jq -n \
  --arg installed_at "${STAMP}" \
  --arg version "${VERSION}" \
  --arg project_file "${PROJECT_FILE}" \
  --arg plugin_path "${PLUGIN_DIR}" \
  --arg tools_path "${INSTALL_ROOT}/tools" \
  --arg specs_path "${INSTALL_ROOT}/specs" \
  --arg skill_path "${CODEX_SKILL_DIR}" \
  --arg codex_home "${CODEX_HOME}" \
  --arg env_path "${CODEX_ENV_FILE}" \
  --arg shell_env_path "${CODEX_SHELL_ENV_FILE}" \
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
    skill_path: $skill_path,
    codex_home: $codex_home,
    env_path: $env_path,
    shell_env_path: $shell_env_path,
    source_release: $source_release,
    checksums: {
      plugin: $plugin_manifest[0].checksums,
      tools: $tools_manifest[0].checksums
    },
    backup_path: $backup_path,
    last_validation_report: null
  }' > "${installed_manifest_temp}"; then
  rm -f "${installed_manifest_temp}"
  exit 2
fi
uecf_commit_output_temp_file "${installed_manifest_temp}" "${INSTALLED_MANIFEST}" "install_local"

uecf_reject_output_file_path "${PROJECT_MANIFEST}" "install_local"
project_manifest_temp="$(uecf_prepare_output_temp_file "${PROJECT_MANIFEST}" "install_local")"
if ! jq -n \
  --arg project_file "${PROJECT_FILE}" \
  --arg plugin_path "${PLUGIN_DIR}" \
  --arg codex_home "${CODEX_HOME}" \
  --arg codex_tools_path "${INSTALL_ROOT}/tools" \
  --arg codex_specs_path "${INSTALL_ROOT}/specs" \
  --arg codex_skill_path "${CODEX_SKILL_DIR}" \
  --arg installed_version "${VERSION}" \
  --arg installed_manifest_path "${INSTALLED_MANIFEST}" \
  --arg default_env_path "${CODEX_ENV_FILE}" \
  --arg shell_env_path "${CODEX_SHELL_ENV_FILE}" \
  '{
    project_file: $project_file,
    plugin_path: $plugin_path,
    codex_home: $codex_home,
    codex_tools_path: $codex_tools_path,
    codex_specs_path: $codex_specs_path,
    codex_skill_path: $codex_skill_path,
    installed_version: $installed_version,
    installed_manifest_path: $installed_manifest_path,
    default_env_path: $default_env_path,
    shell_env_path: $shell_env_path
  }' > "${project_manifest_temp}"; then
  rm -f "${project_manifest_temp}"
  exit 2
fi
uecf_commit_output_temp_file "${project_manifest_temp}" "${PROJECT_MANIFEST}" "install_local"

uecf_reject_output_file_path "${LOG_FILE}" "install_local"
{
  echo "installed_at=${STAMP}"
  echo "version=${VERSION}"
  echo "project_file=${PROJECT_FILE}"
  echo "plugin_path=${PLUGIN_DIR}"
  echo "tools_path=${INSTALL_ROOT}/tools"
  echo "specs_path=${INSTALL_ROOT}/specs"
  echo "skill_path=${CODEX_SKILL_DIR}"
  echo "codex_agents_path=${CODEX_AGENTS_FILE}"
} | uecf_append_file_from_stdin "${LOG_FILE}" "install_local"

if [ "${RUN_COMMANDLET_CHECK}" = true ]; then
  UECF_PROJECT_FILE="${PROJECT_FILE}" "${INSTALL_ROOT}/tools/ue/hello.sh" >/dev/null
fi

echo "${INSTALLED_MANIFEST}"
