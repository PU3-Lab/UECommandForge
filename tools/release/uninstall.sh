#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/common.sh"

PROJECT_FILE="${UECF_PROJECT:-${PROJECT_FILE:-}}"
AGENTS=()
REMOVE_AGENT_TOOLS=false
REMOVE_SPECS=false

while [ $# -gt 0 ]; do
  case "$1" in
    --project)
      PROJECT_FILE="$2"
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
    --remove-agent-tools|--remove-codex-tools)
      REMOVE_AGENT_TOOLS="$2"
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

case "${REMOVE_AGENT_TOOLS}" in true|false) ;; *) echo "[uninstall] --remove-agent-tools must be true or false" >&2; exit 2 ;; esac
case "${REMOVE_SPECS}" in true|false) ;; *) echo "[uninstall] --remove-specs must be true or false" >&2; exit 2 ;; esac

if [ -z "${PROJECT_FILE}" ] || [ ! -f "${PROJECT_FILE}" ]; then
  echo "[uninstall] project file not found: ${PROJECT_FILE}" >&2
  exit 2
fi

if [ "${#AGENTS[@]}" -eq 0 ]; then
  echo "[uninstall] 최소 하나의 에이전트 플래그가 필요합니다: --codex | --claude | --antigravity" >&2
  exit 2
fi

# 중복 제거
DEDUP_AGENTS=()
for agent in "${AGENTS[@]}"; do
  case " ${DEDUP_AGENTS[*]} " in *" ${agent} "*) ;; *) DEDUP_AGENTS+=("${agent}") ;; esac
done
AGENTS=("${DEDUP_AGENTS[@]}")

agent_home_for() {
  case "$1" in
    codex) printf '%s/.codex' "${HOME}" ;;
    claude) printf '%s/.claude' "${HOME}" ;;
    antigravity) printf '%s/.gemini' "${HOME}" ;;
    *) echo "[uninstall] unknown agent: $1" >&2; exit 2 ;;
  esac
}

agent_instructions_filename_for() {
  case "$1" in
    codex) printf 'AGENTS.md' ;;
    claude) printf 'CLAUDE.md' ;;
    antigravity) printf 'GEMINI.md' ;;
    *) echo "[uninstall] unknown agent: $1" >&2; exit 2 ;;
  esac
}

uecf_reject_link_ancestors "${PROJECT_FILE}" "uninstall"
uecf_reject_link_ancestors "$(dirname "${PROJECT_FILE}")" "uninstall"
PROJECT_FILE="$(cd "$(dirname "${PROJECT_FILE}")" && pwd)/$(basename "${PROJECT_FILE}")"
PROJECT_DIR="$(cd "$(dirname "${PROJECT_FILE}")" && pwd)"
LOG_DIR="${PROJECT_DIR}/Saved/UECommandForge"
LOG_FILE="${LOG_DIR}/uninstall.log"
STAMP="$(date -u +%Y%m%dT%H%M%SZ)"

reject_symlink_path() {
  uecf_reject_link_path "$1" "uninstall"
}

PROJECT_LINK_DIR="${PROJECT_DIR}/UECommandForge"
PLUGIN_DIR="${PROJECT_DIR}/Plugins/UECommandForge"
PROJECT_MANIFEST="${PROJECT_LINK_DIR}/uecommandforge-project.json"

reject_symlink_path "${PROJECT_DIR}/Plugins"
reject_symlink_path "${PLUGIN_DIR}"
reject_symlink_path "${PROJECT_LINK_DIR}"
reject_symlink_path "${PROJECT_DIR}/Saved"
reject_symlink_path "${LOG_DIR}"

uecf_reject_link_ancestors "${PLUGIN_DIR}" "uninstall"
uecf_reject_link_tree "${PLUGIN_DIR}" "uninstall"
uecf_reject_output_file_path "${PROJECT_MANIFEST}" "uninstall"
uecf_reject_output_file_path "${LOG_FILE}" "uninstall"

if [ ! -f "${PROJECT_MANIFEST}" ]; then
  echo "[uninstall] project manifest not found; refusing to uninstall" >&2
  exit 2
fi

# 프로젝트 manifest 정합성 검증
jq -e \
  --arg project_file "${PROJECT_FILE}" \
  --arg plugin_path "${PLUGIN_DIR}" \
  '.project_file == $project_file and .plugin_path == $plugin_path' \
  "${PROJECT_MANIFEST}" >/dev/null

uninstall_for_agent() {
  local AGENT="$1"
  local AGENT_HOME
  AGENT_HOME="$(agent_home_for "${AGENT}")"
  uecf_reject_link_ancestors "${AGENT_HOME}" "uninstall"
  AGENT_HOME="$(mkdir -p "${AGENT_HOME}" && cd "${AGENT_HOME}" && pwd)"
  local INSTALL_ROOT="${AGENT_HOME}/UECommandForge"
  local SKILL_DIR="${AGENT_HOME}/skills/uecommandforge"
  local INSTALLED_MANIFEST="${INSTALL_ROOT}/uecommandforge-installed.json"
  local INSTRUCTIONS_FILE="${AGENT_HOME}/$(agent_instructions_filename_for "${AGENT}")"

  reject_symlink_path "${INSTALL_ROOT}"
  reject_symlink_path "${INSTALL_ROOT}/tools"
  reject_symlink_path "${INSTALL_ROOT}/specs"
  reject_symlink_path "${AGENT_HOME}/skills"
  reject_symlink_path "${SKILL_DIR}"
  reject_symlink_path "${INSTRUCTIONS_FILE}"

  uecf_reject_link_ancestors "${INSTALL_ROOT}/tools" "uninstall"
  uecf_reject_link_ancestors "${INSTALL_ROOT}/specs" "uninstall"
  uecf_reject_link_ancestors "${SKILL_DIR}" "uninstall"

  uecf_reject_link_tree "${INSTALL_ROOT}/tools" "uninstall"
  uecf_reject_link_tree "${INSTALL_ROOT}/specs" "uninstall"
  uecf_reject_link_tree "${SKILL_DIR}" "uninstall"
  uecf_reject_output_file_path "${INSTALLED_MANIFEST}" "uninstall"
  uecf_reject_output_file_path "${INSTALL_ROOT}/uecommandforge.env" "uninstall"
  uecf_reject_output_file_path "${INSTALL_ROOT}/uecommandforge.env.sh" "uninstall"

  if [ ! -f "${INSTALLED_MANIFEST}" ]; then
    echo "[uninstall] installed manifest not found for ${AGENT}; skipping agent-specific removal" >&2
    return 0
  fi

  # fallback을 포함한 jq 검증
  local TOOLS_PATH SPECS_PATH SKILL_PATH AGENT_HOME_VAL
  TOOLS_PATH="$(jq -r '.tools_path // .codex_tools_path' "${INSTALLED_MANIFEST}")"
  SPECS_PATH="$(jq -r '.specs_path // .codex_specs_path' "${INSTALLED_MANIFEST}")"
  SKILL_PATH="$(jq -r '.skill_path // .codex_skill_path' "${INSTALLED_MANIFEST}")"
  AGENT_HOME_VAL="$(jq -r '.agent_home // .codex_home' "${INSTALLED_MANIFEST}")"

  jq -e \
    --arg project_file "${PROJECT_FILE}" \
    --arg plugin_path "${PLUGIN_DIR}" \
    --arg tools_path "${TOOLS_PATH}" \
    --arg specs_path "${SPECS_PATH}" \
    --arg skill_path "${SKILL_PATH}" \
    --arg agent_home "${AGENT_HOME_VAL}" \
    '.project_file == $project_file
     and .plugin_path == $plugin_path
     and (.tools_path // .codex_tools_path) == $tools_path
     and (.specs_path // .codex_specs_path) == $specs_path
     and ((.skill_path // .codex_skill_path // $skill_path) == $skill_path)
     and (.agent_home // .codex_home) == $agent_home' \
    "${INSTALLED_MANIFEST}" >/dev/null

  # 지시 파일에서 관리 블록(옛 마커/새 마커 모두) 제거
  if [ -f "${INSTRUCTIONS_FILE}" ] && [ -w "${INSTRUCTIONS_FILE}" ]; then
    local begin_count_old
    local begin_count_new
    begin_count_old="$(grep -c 'BEGIN UECOMMANDFORGE CODEX INSTRUCTIONS' "${INSTRUCTIONS_FILE}" || true)"
    begin_count_new="$(grep -c 'BEGIN UECOMMANDFORGE INSTRUCTIONS' "${INSTRUCTIONS_FILE}" || true)"
    if [ "${begin_count_old}" -gt 0 ] || [ "${begin_count_new}" -gt 0 ]; then
      local temp_file
      temp_file="$(mktemp "${AGENT_HOME}/.instructions.uninstall.XXXXXX")"
      awk '
        /BEGIN UECOMMANDFORGE (CODEX )?INSTRUCTIONS/ { skipping = 1; next }
        /END UECOMMANDFORGE (CODEX )?INSTRUCTIONS/ { skipping = 0; next }
        skipping != 1 { print }
      ' "${INSTRUCTIONS_FILE}" > "${temp_file}"
      mv "${temp_file}" "${INSTRUCTIONS_FILE}"
    fi
  fi

  if [ "${REMOVE_AGENT_TOOLS}" = true ]; then
    uecf_reject_link_tree "${INSTALL_ROOT}/tools" "uninstall"
    rm -rf "${INSTALL_ROOT}/tools"
  fi
  if [ "${REMOVE_SPECS}" = true ]; then
    uecf_reject_link_tree "${INSTALL_ROOT}/specs" "uninstall"
    rm -rf "${INSTALL_ROOT}/specs"
  fi
  if [ "${REMOVE_AGENT_TOOLS}" = true ] && [ "${REMOVE_SPECS}" = true ]; then
    uecf_reject_link_tree "${SKILL_DIR}" "uninstall"
    rm -rf "${SKILL_DIR}"
    uecf_reject_output_file_path "${INSTALL_ROOT}/uecommandforge.env" "uninstall"
    uecf_reject_output_file_path "${INSTALL_ROOT}/uecommandforge.env.sh" "uninstall"
    uecf_reject_output_file_path "${INSTALLED_MANIFEST}" "uninstall"
    rm -f "${INSTALL_ROOT}/uecommandforge.env" \
      "${INSTALL_ROOT}/uecommandforge.env.sh" \
      "${INSTALLED_MANIFEST}"
  fi
}

for agent in "${AGENTS[@]}"; do
  uninstall_for_agent "${agent}"
done

# 프로젝트 영역의 리소스 삭제 (1회)
mkdir -p "${LOG_DIR}"
uecf_reject_link_tree "${PLUGIN_DIR}" "uninstall"
uecf_reject_output_file_path "${PROJECT_MANIFEST}" "uninstall"

rm -rf "${PLUGIN_DIR}"
rm -f "${PROJECT_MANIFEST}"
rmdir "${PROJECT_LINK_DIR}" 2>/dev/null || true

uecf_reject_output_file_path "${LOG_FILE}" "uninstall"
{
  echo "uninstalled_at=${STAMP}"
  echo "project_file=${PROJECT_FILE}"
  echo "removed_agent_tools=${REMOVE_AGENT_TOOLS}"
  echo "removed_specs=${REMOVE_SPECS}"
  echo "uninstalled_agents=${AGENTS[*]}"
} | uecf_append_file_from_stdin "${LOG_FILE}" "uninstall"

echo "${LOG_FILE}"
