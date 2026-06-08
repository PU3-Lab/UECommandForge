#!/usr/bin/env bash
# 사용법: run_commandlet.sh <CommandletName> [extra-args...]
# Result JSON은 대상 project의 Saved/CodexReports/<Commandlet>_<UTC>.json에 기록된다.

set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/ue_env.sh"

if [ $# -lt 1 ]; then
  echo "사용법: $0 <CommandletName> [extra args...]" >&2
  exit 2
fi

COMMANDLET="$1"
shift

contains_exec_python() {
  local value="$1"
  local normalized
  case "${value}" in
    *"executepython"*|*"pythonscript"*|*"import unreal"*|*"unreal."*) return 0 ;;
  esac
  normalized="$(printf '%s' "${value}" | tr '\011\012\015;,' '     ')"
  normalized="${normalized//\"/ }"
  normalized="${normalized//\'/ }"
  case " ${normalized} " in
    *" py "*) return 0 ;;
  esac
  return 1
}

reject_unreal_python_args() {
  local previous_was_exec_cmds=false
  local raw lower exec_value

  raw="${COMMANDLET}"
  lower="$(printf '%s' "${raw}" | tr '[:upper:]' '[:lower:]')"
  if contains_exec_python "${lower}"; then
    echo "[run_commandlet] Unreal Python access is not allowed; use UECommandForge commandlets/wrappers instead." >&2
    exit 2
  fi

  for raw in "$@"; do
    lower="$(printf '%s' "${raw}" | tr '[:upper:]' '[:lower:]')"

    case "${lower}" in
      *"-executepythonscript"*|*"-executepythoncommand"*|*"pythonscriptplugin"*|*"import unreal"*|*"unreal."*|*".py"*)
        echo "[run_commandlet] Unreal Python access is not allowed; use UECommandForge commandlets/wrappers instead." >&2
        exit 2
        ;;
    esac

    if [ "${previous_was_exec_cmds}" = true ] && contains_exec_python "${lower}"; then
      echo "[run_commandlet] Unreal Python access is not allowed; use UECommandForge commandlets/wrappers instead." >&2
      exit 2
    fi

    previous_was_exec_cmds=false
    case "${lower}" in
      -execcmds)
        previous_was_exec_cmds=true
        ;;
      -execcmds=*)
        exec_value="${lower#-execcmds=}"
        if contains_exec_python "${exec_value}"; then
          echo "[run_commandlet] Unreal Python access is not allowed; use UECommandForge commandlets/wrappers instead." >&2
          exit 2
        fi
        ;;
    esac
  done
}

if [ ! -f "${PROJECT_FILE}" ]; then
  echo "[run_commandlet] Project file을 찾을 수 없습니다: ${PROJECT_FILE}" >&2
  exit 2
fi

PROJECT_DIR="$(cd "$(dirname "${PROJECT_FILE}")" && pwd)"
REPORT_DIR="${UECF_REPORT_DIR:-${PROJECT_DIR}/Saved/CodexReports}"
STAMP="$(date -u +%Y%m%dT%H%M%SZ)"
OUTPUT_JSON="${REPORT_DIR}/${COMMANDLET}_${STAMP}.json"
TIMEOUT_SEC="${UE_COMMANDLET_TIMEOUT:-300}"

case "${TIMEOUT_SEC}" in
  ''|*[!0-9]*)
    echo "[run_commandlet] UE_COMMANDLET_TIMEOUT은 초 단위 정수여야 합니다: ${TIMEOUT_SEC}" >&2
    exit 2
    ;;
esac

reject_unreal_python_args "$@"
mkdir -p "${REPORT_DIR}"

run_unreal_commandlet() {
  local start_epoch
  start_epoch=$(date +%s)
  echo "[run_commandlet] [PERF] ${COMMANDLET} 시작 시각: $(date -u +%Y-%m-%dT%H:%M:%SZ) (${start_epoch})" >&2

  local need_shaders=false
  case "${COMMANDLET}" in
    CreateBlueprint*|SetBlueprintDefaults|CompileBlueprints|CreateAIFlow|ValidateAIFlow|PlaceActor)
      need_shaders=true
      ;;
  esac

  local extra_flags=("-NoSound")
  if [ "${need_shaders}" = false ]; then
    extra_flags+=("-NoShaderCompile" "-SkipShaderCompile")
  fi

  "${UNREAL_EDITOR_CMD}" "${PROJECT_FILE}" \
    -run="${COMMANDLET}" \
    -Output="${OUTPUT_JSON}" \
    -unattended -nop4 -nosplash -nullrhi -log -stdout -FullStdOutLogOutput \
    "${extra_flags[@]}" \
    "$@"
  local exit_code=$?

  local end_epoch
  end_epoch=$(date +%s)
  local duration=$((end_epoch - start_epoch))
  echo "[run_commandlet] [PERF] ${COMMANDLET} 종료 시각: $(date -u +%Y-%m-%dT%H:%M:%SZ) (${end_epoch}) | 소요시간: ${duration}초 | ExitCode: ${exit_code}" >&2

  return ${exit_code}
}

set +e
if [ "${TIMEOUT_SEC}" -eq 0 ]; then
  run_unreal_commandlet "$@"
  UE_EXIT=$?
else
  echo "[run_commandlet] ${COMMANDLET} 시작 (timeout: ${TIMEOUT_SEC}s)" >&2
  run_unreal_commandlet "$@" &
  UE_PID=$!
  ELAPSED=0

  while kill -0 "${UE_PID}" 2>/dev/null; do
    if [ "${ELAPSED}" -ge "${TIMEOUT_SEC}" ]; then
      echo "[run_commandlet] FAIL: ${COMMANDLET}가 ${TIMEOUT_SEC}초를 초과했습니다." >&2
      kill "${UE_PID}" 2>/dev/null || true
      sleep 5
      kill -0 "${UE_PID}" 2>/dev/null && kill -KILL "${UE_PID}" 2>/dev/null
      wait "${UE_PID}" 2>/dev/null
      exit 124
    fi

    sleep 1
    ELAPSED=$((ELAPSED + 1))
  done

  wait "${UE_PID}"
  UE_EXIT=$?
fi
set -e

if [ ! -f "${OUTPUT_JSON}" ]; then
  echo "[run_commandlet] Result JSON이 없습니다: ${OUTPUT_JSON}" >&2
  exit 5
fi

echo "${OUTPUT_JSON}"
exit ${UE_EXIT}
