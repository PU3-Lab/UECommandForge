#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/common.sh"
PROJECT_FILE="${UECF_PROJECT:-${PROJECT_FILE:-}}"
PLUGIN_PACKAGE=""
TOOLS_PACKAGE=""
AGENTS=()
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
    --codex)
      AGENTS+=("codex")
      shift
      ;;
    --claude)
      AGENTS+=("claude")
      shift
      ;;
    --antigravity)
      AGENTS+=("antigravity")
      shift
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
  echo "Usage: $0 --project <path.uproject> --plugin-package <zip> --tools-package <zip> --codex | --claude | --antigravity" >&2
  exit 2
fi

if [ "${#AGENTS[@]}" -eq 0 ]; then
  echo "[install_local] 최소 하나의 에이전트 플래그가 필요합니다: --codex | --claude | --antigravity" >&2
  exit 2
fi

# 중복 플래그 제거(순서 유지)
DEDUP_AGENTS=()
for agent in "${AGENTS[@]}"; do
  case " ${DEDUP_AGENTS[*]} " in *" ${agent} "*) ;; *) DEDUP_AGENTS+=("${agent}") ;; esac
done
AGENTS=("${DEDUP_AGENTS[@]}")

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

agent_home_for() {
  case "$1" in
    codex) printf '%s/.codex' "${HOME}" ;;
    claude) printf '%s/.claude' "${HOME}" ;;
    antigravity) printf '%s/.gemini' "${HOME}" ;;
    *) echo "[install_local] unknown agent: $1" >&2; exit 2 ;;
  esac
}

agent_instructions_filename_for() {
  case "$1" in
    codex) printf 'AGENTS.md' ;;
    claude) printf 'CLAUDE.md' ;;
    antigravity) printf 'GEMINI.md' ;;
    *) echo "[install_local] unknown agent: $1" >&2; exit 2 ;;
  esac
}

uecf_reject_link_ancestors "${PROJECT_FILE}" "install_local"
uecf_reject_link_ancestors "$(dirname "${PROJECT_FILE}")" "install_local"
PROJECT_FILE="$(cd "$(dirname "${PROJECT_FILE}")" && pwd)/$(basename "${PROJECT_FILE}")"
PROJECT_DIR="$(cd "$(dirname "${PROJECT_FILE}")" && pwd)"
PLUGIN_DIR="${PROJECT_DIR}/Plugins/UECommandForge"
PROJECT_LINK_DIR="${PROJECT_DIR}/UECommandForge"
LOG_DIR="${PROJECT_DIR}/Saved/UECommandForge"
LOG_FILE="${LOG_DIR}/install.log"
PROJECT_MANIFEST="${PROJECT_LINK_DIR}/uecommandforge-project.json"
STAMP="$(date -u +%Y%m%dT%H%M%SZ)-$$"
BACKUP_ROOT="${PROJECT_DIR}/Saved/UECommandForge/Backups/${STAMP}"

reject_symlink_path() {
  uecf_reject_link_path "$1" "install_local"
}

# 공통 프로젝트 심링크/경로 거부 가드
reject_symlink_path "${PROJECT_DIR}/Plugins"
reject_symlink_path "${PROJECT_DIR}/Plugins/UECommandForge"
reject_symlink_path "${PROJECT_DIR}/UECommandForge"
reject_symlink_path "${PROJECT_DIR}/Saved"
reject_symlink_path "${PROJECT_DIR}/Saved/UECommandForge"
uecf_reject_output_file_path "${PROJECT_MANIFEST}" "install_local"
uecf_reject_output_file_path "${LOG_FILE}" "install_local"
uecf_reject_link_tree "${PLUGIN_DIR}" "install_local"

# 기존 설치된 리소스 검증
require_managed_existing_targets_for_agent() {
  local AGENT="$1"
  local AGENT_HOME
  AGENT_HOME="$(agent_home_for "${AGENT}")"
  local AGENT_INSTALL_ROOT="${AGENT_HOME}/UECommandForge"
  local AGENT_SKILL_DIR="${AGENT_HOME}/skills/uecommandforge"
  local AGENT_ENV_FILE="${AGENT_INSTALL_ROOT}/uecommandforge.env"
  local AGENT_SHELL_ENV_FILE="${AGENT_INSTALL_ROOT}/uecommandforge.env.sh"
  local AGENT_MANIFEST="${AGENT_INSTALL_ROOT}/uecommandforge-installed.json"

  local has_codex_state=false
  if [ -e "${AGENT_INSTALL_ROOT}/tools" ] || [ -e "${AGENT_INSTALL_ROOT}/specs" ] \
    || [ -e "${AGENT_SKILL_DIR}" ] \
    || [ -e "${AGENT_ENV_FILE}" ] || [ -e "${AGENT_SHELL_ENV_FILE}" ] \
    || [ -e "${AGENT_MANIFEST}" ]; then
    has_codex_state=true
  fi

  if [ "${has_codex_state}" = true ]; then
    if [ ! -f "${AGENT_MANIFEST}" ]; then
      echo "[install_local] existing ${AGENT} install targets are unmanaged; refusing to overwrite" >&2
      exit 2
    fi
    jq -e \
      --arg project_file "${PROJECT_FILE}" \
      --arg plugin_path "${PLUGIN_DIR}" \
      --arg tools_path "${AGENT_INSTALL_ROOT}/tools" \
      --arg specs_path "${AGENT_INSTALL_ROOT}/specs" \
      --arg skill_path "${AGENT_SKILL_DIR}" \
      '.project_file == $project_file
       and .plugin_path == $plugin_path
       and .tools_path == $tools_path
       and .specs_path == $specs_path
       and ((.skill_path // $skill_path) == $skill_path)' \
      "${AGENT_MANIFEST}" >/dev/null
    if [ -e "${AGENT_SKILL_DIR}" ]; then
      jq -e \
        --arg skill_path "${AGENT_SKILL_DIR}" \
        '.skill_path == $skill_path' \
        "${AGENT_MANIFEST}" >/dev/null
    fi
  fi
}

require_managed_existing_project_targets() {
  local has_project_state=false
  if [ -e "${PLUGIN_DIR}" ] || [ -e "${PROJECT_LINK_DIR}" ] || [ -e "${PROJECT_MANIFEST}" ]; then
    has_project_state=true
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
}

require_managed_existing_project_targets
for agent in "${AGENTS[@]}"; do
  require_managed_existing_targets_for_agent "${agent}"
done

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
    echo "[install_local] 에이전트 지침 소스 파일이 없습니다: ${instructions_file}" >&2
    exit 2
  fi
  require_ordered_marker_pair \
    "${instructions_file}" \
    "BEGIN UECOMMANDFORGE MANAGED CONTENT" \
    "END UECOMMANDFORGE MANAGED CONTENT" \
    required \
    "[install_local] 에이전트 지침 소스 파일에 올바른 관리 마커가 없습니다: ${instructions_file}"
}

require_agent_instructions_appendable() {
  local AGENT_HOME="$1"
  local INSTRUCTIONS_FILE="$2"

  if [ ! -w "${AGENT_HOME}" ]; then
    echo "[install_local] 에이전트 홈 디렉터리에 쓰기 권한이 없습니다: ${AGENT_HOME}" >&2
    exit 2
  fi

  local probe_file
  probe_file="$(mktemp "${AGENT_HOME}/.instructions.preflight.XXXXXX")"
  if ! mv "${probe_file}" "${probe_file}.rename"; then
    rm -f "${probe_file}" "${probe_file}.rename"
    echo "[install_local] 에이전트 홈에서 지침 파일을 원자적으로 수정할 수 없습니다: ${AGENT_HOME}" >&2
    exit 2
  fi
  rm -f "${probe_file}.rename"

  if [ -e "${INSTRUCTIONS_FILE}" ]; then
    if [ ! -f "${INSTRUCTIONS_FILE}" ]; then
      echo "[install_local] 에이전트 지침 경로가 일반 파일이 아닙니다: ${INSTRUCTIONS_FILE}" >&2
      exit 2
    fi
    if [ ! -w "${INSTRUCTIONS_FILE}" ]; then
      echo "[install_local] 에이전트 지침 파일에 쓰기 권한이 없습니다: ${INSTRUCTIONS_FILE}" >&2
      echo "[install_local] 기존 지침 파일을 보존하기 위해 설치를 중단합니다. 권한을 수정하고 재시도하세요." >&2
      exit 2
    fi
    require_ordered_marker_pair \
      "${INSTRUCTIONS_FILE}" \
      "BEGIN UECOMMANDFORGE INSTRUCTIONS" \
      "END UECOMMANDFORGE INSTRUCTIONS" \
      optional \
      "[install_local] 기존 지침 파일의 UECommandForge 마커가 잘못되었습니다: ${INSTRUCTIONS_FILE}"
  fi
}

append_agent_instructions() {
  local AGENT_HOME="$1"
  local INSTRUCTIONS_FILE="$2"
  local temp_file
  local begin_count=0
  local instructions_source="${TOOLS_ROOT}/specs/agent/unreal-automation-agents.md"
  require_instruction_source "${instructions_source}"
  reject_symlink_path "${INSTRUCTIONS_FILE}"

  temp_file="$(mktemp "${AGENT_HOME}/.instructions.XXXXXX")"
  if [ -f "${INSTRUCTIONS_FILE}" ]; then
    cp -p "${INSTRUCTIONS_FILE}" "${temp_file}"
    # 옛/새 마커가 모두 유효한 쌍인지 검사
    local begin_count_old
    local begin_count_new
    begin_count_old="$(grep -c 'BEGIN UECOMMANDFORGE CODEX INSTRUCTIONS' "${INSTRUCTIONS_FILE}" || true)"
    begin_count_new="$(grep -c 'BEGIN UECOMMANDFORGE INSTRUCTIONS' "${INSTRUCTIONS_FILE}" || true)"
    
    if [ "${begin_count_old}" -gt 0 ]; then
      require_ordered_marker_pair \
        "${INSTRUCTIONS_FILE}" \
        "BEGIN UECOMMANDFORGE CODEX INSTRUCTIONS" \
        "END UECOMMANDFORGE CODEX INSTRUCTIONS" \
        optional \
        "[install_local] 기존 지침 파일의 옛 UECommandForge 마커가 잘못되었습니다: ${INSTRUCTIONS_FILE}"
    fi
    if [ "${begin_count_new}" -gt 0 ]; then
      require_ordered_marker_pair \
        "${INSTRUCTIONS_FILE}" \
        "BEGIN UECOMMANDFORGE INSTRUCTIONS" \
        "END UECOMMANDFORGE INSTRUCTIONS" \
        optional \
        "[install_local] 기존 지침 파일의 새 UECommandForge 마커가 잘못되었습니다: ${INSTRUCTIONS_FILE}"
    fi

    # 옛 마커 및 새 마커 블록 제거
    if [ "${begin_count_old}" -eq 1 ] || [ "${begin_count_new}" -eq 1 ]; then
      awk '
        /BEGIN UECOMMANDFORGE (CODEX )?INSTRUCTIONS/ { skipping = 1; next }
        /END UECOMMANDFORGE (CODEX )?INSTRUCTIONS/ { skipping = 0; next }
        skipping != 1 { print }
      ' "${INSTRUCTIONS_FILE}" > "${temp_file}.stripped"
      cat "${temp_file}.stripped" > "${temp_file}"
      rm -f "${temp_file}.stripped"
    fi
  fi

  if [ -s "${INSTRUCTIONS_FILE}" ]; then
    printf '\n' >> "${temp_file}"
  fi

cat >> "${temp_file}" <<'INSTR'
<!-- BEGIN UECOMMANDFORGE INSTRUCTIONS -->
INSTR
  awk '
    /BEGIN UECOMMANDFORGE MANAGED CONTENT/ { copying = 1; next }
    /END UECOMMANDFORGE MANAGED CONTENT/ { copying = 0; next }
    copying == 1 { print }
  ' "${instructions_source}" >> "${temp_file}"
  printf '\n' >> "${temp_file}"
cat >> "${temp_file}" <<'INSTR'
<!-- END UECOMMANDFORGE INSTRUCTIONS -->
INSTR

  mv "${temp_file}" "${INSTRUCTIONS_FILE}"
}

WORK_DIR="$(mktemp -d)"
cleanup() {
  rm -rf "${WORK_DIR}"
}
trap cleanup EXIT

PLUGIN_STAGED_PACKAGE="$(stage_package_for_install "${PLUGIN_PACKAGE}" "plugin")"
# 이 시점엔 TOOLS_ROOT 가 전역 변수로 필요함. append_agent_instructions 등에서 참조하기 때문.
TOOLS_ROOT="${WORK_DIR}/tools"
PLUGIN_ROOT="${WORK_DIR}/plugin"
mkdir -p "${PLUGIN_ROOT}" "${TOOLS_ROOT}"
unzip -q "${PLUGIN_STAGED_PACKAGE}" -d "${PLUGIN_ROOT}"

TOOLS_STAGED_PACKAGE="$(stage_package_for_install "${TOOLS_PACKAGE}" "tools")"
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

jq -e --arg version "${VERSION}" '.version == $version' "${PLUGIN_MANIFEST}" >/dev/null
jq -e '.package_type == "plugin"' "${PLUGIN_MANIFEST}" >/dev/null
jq -e --arg version "${VERSION}" '.version == $version' "${TOOLS_MANIFEST}" >/dev/null
jq -e '.package_type == "tools"' "${TOOLS_MANIFEST}" >/dev/null
jq -e --slurpfile plugin_manifest "${PLUGIN_MANIFEST}" '.release_channel == $plugin_manifest[0].release_channel' "${TOOLS_MANIFEST}" >/dev/null

require_instruction_source "${TOOLS_ROOT}/specs/agent/unreal-automation-agents.md"

# 프로젝트 리소스 백업 및 설치 (1회)
mkdir -p "${LOG_DIR}" "${PROJECT_LINK_DIR}"

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
else
  uecf_reject_link_tree "${PLUGIN_DIR}" "install_local"
  rm -rf "${PLUGIN_DIR}"
fi

mkdir -p "$(dirname "${PLUGIN_DIR}")"
uecf_reject_link_tree "${PLUGIN_DIR}" "install_local"
cp -R "${PLUGIN_ROOT}" "${PLUGIN_DIR}"
rm -f "${PLUGIN_DIR}/uecommandforge-manifest.json" \
  "${PLUGIN_DIR}/checksums.txt" \
  "${PLUGIN_DIR}/install.md" \
  "${PLUGIN_DIR}/release-notes.md" \
  "${PLUGIN_DIR}/validation-report.json"

# 에이전트별 설치 함수
install_for_agent() {
  local AGENT="$1"
  local AGENT_HOME
  AGENT_HOME="$(agent_home_for "${AGENT}")"
  uecf_reject_link_ancestors "${AGENT_HOME}" "install_local"
  AGENT_HOME="$(mkdir -p "${AGENT_HOME}" && cd "${AGENT_HOME}" && pwd -P)"
  local INSTALL_ROOT="${AGENT_HOME}/UECommandForge"
  local SKILL_DIR="${AGENT_HOME}/skills/uecommandforge"
  local INSTRUCTIONS_FILE="${AGENT_HOME}/$(agent_instructions_filename_for "${AGENT}")"
  local AGENT_ENV_FILE="${INSTALL_ROOT}/uecommandforge.env"
  local AGENT_SHELL_ENV_FILE="${INSTALL_ROOT}/uecommandforge.env.sh"
  local INSTALLED_MANIFEST="${INSTALL_ROOT}/uecommandforge-installed.json"

  # 에이전트 파일 거부 가드
  reject_symlink_path "${INSTALL_ROOT}"
  reject_symlink_path "${INSTALL_ROOT}/tools"
  reject_symlink_path "${INSTALL_ROOT}/specs"
  reject_symlink_path "${AGENT_HOME}/skills"
  reject_symlink_path "${SKILL_DIR}"
  reject_symlink_path "${INSTRUCTIONS_FILE}"
  uecf_reject_output_file_path "${AGENT_ENV_FILE}" "install_local"
  uecf_reject_output_file_path "${AGENT_SHELL_ENV_FILE}" "install_local"
  uecf_reject_output_file_path "${INSTALLED_MANIFEST}" "install_local"
  uecf_reject_link_tree "${INSTALL_ROOT}/tools" "install_local"
  uecf_reject_link_tree "${INSTALL_ROOT}/specs" "install_local"
  uecf_reject_link_tree "${SKILL_DIR}" "install_local"

  require_agent_instructions_appendable "${AGENT_HOME}" "${INSTRUCTIONS_FILE}"
  mkdir -p "${INSTALL_ROOT}"

  if [ "${BACKUP}" = true ]; then
    if [ -d "${INSTALL_ROOT}/tools" ]; then
      uecf_reject_link_tree "${INSTALL_ROOT}/tools" "install_local"
      uecf_reject_link_ancestors "${BACKUP_ROOT}/${AGENT}/tools" "install_local"
      mkdir -p "${BACKUP_ROOT}/${AGENT}"
      mv "${INSTALL_ROOT}/tools" "${BACKUP_ROOT}/${AGENT}/tools"
    fi
    if [ -d "${INSTALL_ROOT}/specs" ]; then
      uecf_reject_link_tree "${INSTALL_ROOT}/specs" "install_local"
      uecf_reject_link_ancestors "${BACKUP_ROOT}/${AGENT}/specs" "install_local"
      mkdir -p "${BACKUP_ROOT}/${AGENT}"
      mv "${INSTALL_ROOT}/specs" "${BACKUP_ROOT}/${AGENT}/specs"
    fi
    if [ -d "${SKILL_DIR}" ]; then
      uecf_reject_link_tree "${SKILL_DIR}" "install_local"
      uecf_reject_link_ancestors "${BACKUP_ROOT}/${AGENT}/skills/uecommandforge" "install_local"
      mkdir -p "${BACKUP_ROOT}/${AGENT}/skills"
      mv "${SKILL_DIR}" "${BACKUP_ROOT}/${AGENT}/skills/uecommandforge"
    fi
  else
    uecf_reject_link_tree "${INSTALL_ROOT}/tools" "install_local"
    uecf_reject_link_tree "${INSTALL_ROOT}/specs" "install_local"
    uecf_reject_link_tree "${SKILL_DIR}" "install_local"
    rm -rf "${INSTALL_ROOT}/tools" "${INSTALL_ROOT}/specs" "${SKILL_DIR}"
  fi

  copy_tree "${TOOLS_ROOT}/tools" "${INSTALL_ROOT}/tools"
  copy_tree "${TOOLS_ROOT}/specs" "${INSTALL_ROOT}/specs"
  copy_tree "${TOOLS_ROOT}/skills/uecommandforge" "${SKILL_DIR}"
  find "${INSTALL_ROOT}/tools" -type f -name '*.sh' -exec chmod +x {} +
  append_agent_instructions "${AGENT_HOME}" "${INSTRUCTIONS_FILE}"

  uecf_reject_output_file_path "${AGENT_ENV_FILE}" "install_local"
  uecf_write_file_from_stdin "${AGENT_ENV_FILE}" "install_local" <<ENV
UECF_PROJECT_FILE=${PROJECT_FILE}
AGENT_HOME=${AGENT_HOME}
UECF_INSTALL_ROOT=${INSTALL_ROOT}
ENV

  uecf_reject_output_file_path "${AGENT_SHELL_ENV_FILE}" "install_local"
  uecf_write_file_from_stdin "${AGENT_SHELL_ENV_FILE}" "install_local" <<ENV
export UECF_PROJECT_FILE=$(uecf_shell_quote "${PROJECT_FILE}")
export AGENT_HOME=$(uecf_shell_quote "${AGENT_HOME}")
export UECF_INSTALL_ROOT=$(uecf_shell_quote "${INSTALL_ROOT}")
ENV

  uecf_reject_output_file_path "${INSTALLED_MANIFEST}" "install_local"
  local installed_manifest_temp
  installed_manifest_temp="$(uecf_prepare_output_temp_file "${INSTALLED_MANIFEST}" "install_local")"
  if ! jq -n \
    --arg installed_at "${STAMP}" \
    --arg version "${VERSION}" \
    --arg project_file "${PROJECT_FILE}" \
    --arg plugin_path "${PLUGIN_DIR}" \
    --arg tools_path "${INSTALL_ROOT}/tools" \
    --arg specs_path "${INSTALL_ROOT}/specs" \
    --arg skill_path "${SKILL_DIR}" \
    --arg agent_home "${AGENT_HOME}" \
    --arg agent_name "${AGENT}" \
    --arg env_path "${AGENT_ENV_FILE}" \
    --arg shell_env_path "${AGENT_SHELL_ENV_FILE}" \
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
      agent_home: $agent_home,
      agent_name: $agent_name,
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

  uecf_reject_output_file_path "${LOG_FILE}" "install_local"
  {
    echo "installed_at=${STAMP}"
    echo "version=${VERSION}"
    echo "project_file=${PROJECT_FILE}"
    echo "plugin_path=${PLUGIN_DIR}"
    echo "agent_name=${AGENT}"
    echo "tools_path=${INSTALL_ROOT}/tools"
    echo "specs_path=${INSTALL_ROOT}/specs"
    echo "skill_path=${SKILL_DIR}"
    echo "agent_instructions_path=${INSTRUCTIONS_FILE}"
  } | uecf_append_file_from_stdin "${LOG_FILE}" "install_local"

  if [ "${RUN_COMMANDLET_CHECK}" = true ]; then
    UECF_PROJECT_FILE="${PROJECT_FILE}" "${INSTALL_ROOT}/tools/ue/hello.sh" >/dev/null
  fi
  echo "${INSTALLED_MANIFEST}"
}

# 에이전트별 순회 설치 진행
FIRST_INSTALLED_MANIFEST=""
for agent in "${AGENTS[@]}"; do
  manifest_result="$(install_for_agent "${agent}")"
  if [ -z "${FIRST_INSTALLED_MANIFEST}" ]; then
    FIRST_INSTALLED_MANIFEST="${manifest_result}"
  fi
done

# 첫 에이전트의 홈 정보 기준으로 대표 project-manifest 작성 (1회)
FIRST_AGENT="${AGENTS[0]}"
FIRST_AGENT_HOME="$(agent_home_for "${FIRST_AGENT}")"
FIRST_INSTALL_ROOT="${FIRST_AGENT_HOME}/UECommandForge"
FIRST_SKILL_DIR="${FIRST_AGENT_HOME}/skills/uecommandforge"
FIRST_ENV_FILE="${FIRST_INSTALL_ROOT}/uecommandforge.env"
FIRST_SHELL_ENV_FILE="${FIRST_INSTALL_ROOT}/uecommandforge.env.sh"
FIRST_MANIFEST_PATH="${FIRST_INSTALL_ROOT}/uecommandforge-installed.json"

project_manifest_temp="$(uecf_prepare_output_temp_file "${PROJECT_MANIFEST}" "install_local")"
if ! jq -n \
  --arg project_file "${PROJECT_FILE}" \
  --arg plugin_path "${PLUGIN_DIR}" \
  --arg agent_home "${FIRST_AGENT_HOME}" \
  --arg tools_path "${FIRST_INSTALL_ROOT}/tools" \
  --arg specs_path "${FIRST_INSTALL_ROOT}/specs" \
  --arg skill_path "${FIRST_SKILL_DIR}" \
  --arg installed_version "${VERSION}" \
  --arg installed_manifest_path "${FIRST_MANIFEST_PATH}" \
  --arg default_env_path "${FIRST_ENV_FILE}" \
  --arg shell_env_path "${FIRST_SHELL_ENV_FILE}" \
  '{
    project_file: $project_file,
    plugin_path: $plugin_path,
    agent_home: $agent_home,
    tools_path: $tools_path,
    specs_path: $specs_path,
    skill_path: $skill_path,
    installed_version: $installed_version,
    installed_manifest_path: $installed_manifest_path,
    default_env_path: $default_env_path,
    shell_env_path: $shell_env_path
  }' > "${project_manifest_temp}"; then
  rm -f "${project_manifest_temp}"
  exit 2
fi
uecf_commit_output_temp_file "${project_manifest_temp}" "${PROJECT_MANIFEST}" "install_local"

echo "installed agents: ${AGENTS[*]}"
echo "${FIRST_INSTALLED_MANIFEST}"
