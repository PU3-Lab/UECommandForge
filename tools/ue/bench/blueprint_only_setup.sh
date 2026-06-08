#!/usr/bin/env bash
# 사용법: ./tools/ue/bench/blueprint_only_setup.sh
# 목적: C++ 단계를 생략한 Blueprint-Only 경로의 시간 측정 및 성능 비교

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"

# UE 환경 변수 로드
# shellcheck disable=SC1091
source "${REPO_ROOT}/tools/ue/ue_env.sh"

# 이전 생성 파일 제거 (Clean)
echo ">>> [CLEAN] 이전 생성된 Blueprint-Only 임시 파일 제거..."
rm -f "${REPO_ROOT}/sample/Content/AI/Characters/BP_BenchCharacterOnly.uasset"

echo "=========================================================="
echo "    [BENCHMARK] Blueprint-Only Setup 시작"
echo "    시작 시각: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
echo "=========================================================="

TOTAL_START=$(date +%s)
EDITOR_LAUNCH_COUNT=0

run_step() {
  local step_name="$1"
  shift
  local cmd=("$@")

  echo ""
  echo ">>> [STEP] ${step_name} 시작"
  local step_start
  step_start=$(date +%s)

  # UnrealEditor-Cmd 실행이 감지되는 명령어인지 확인하여 카운트 증가
  if [[ "${cmd[0]}" == *UnrealEditor-Cmd* || "${cmd[0]}" == *run_commandlet.sh* || "${cmd[0]}" == *create_blueprint.sh* || "${cmd[0]}" == *set_blueprint_defaults.sh* ]]; then
    EDITOR_LAUNCH_COUNT=$((EDITOR_LAUNCH_COUNT + 1))
  fi

  set +e
  "${cmd[@]}"
  local exit_code=$?
  set -e

  local step_end
  step_end=$(date +%s)
  local step_duration=$((step_end - step_start))

  echo "<<< [STEP] ${step_name} 완료 | 소요시간: ${step_duration}초 | ExitCode: ${exit_code}"

  if [ ${exit_code} -ne 0 ]; then
    echo "ERROR: [STEP] ${step_name} 실패로 인해 벤치마크 중단" >&2
    exit ${exit_code}
  fi
}

# 1단계: 블루프린트 생성
run_step "1. 블루프린트 생성" \
  "${REPO_ROOT}/tools/ue/create_blueprint.sh" "${REPO_ROOT}/specs/bench/bp_character_only.json"

# 2단계: 컴포넌트 추가 및 디폴트 설정
run_step "2. 컴포넌트 추가 및 디폴트 설정" \
  "${REPO_ROOT}/tools/ue/set_blueprint_defaults.sh" "${REPO_ROOT}/specs/bench/bp_defaults_only.json"

TOTAL_END=$(date +%s)
TOTAL_DURATION=$((TOTAL_END - TOTAL_START))

echo ""
echo "=========================================================="
echo "    [BENCHMARK] Blueprint-Only Setup 완료"
echo "    종료 시각: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
echo "    총 소요 시간: ${TOTAL_DURATION}초"
echo "    UnrealEditor-Cmd 기동 횟수: ${EDITOR_LAUNCH_COUNT}회"
echo "=========================================================="
