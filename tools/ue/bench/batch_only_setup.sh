#!/usr/bin/env bash
# 사용법: ./tools/ue/bench/batch_only_setup.sh
# 목적: Blueprint-Only 변경 사항을 Batch BP 생성 1회 세션으로 완료하여 극한의 시간 단축 측정

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
echo "    [BENCHMARK] Batch Blueprint-Only Setup 시작"
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

  if [[ "${cmd[0]}" == *UnrealEditor-Cmd* || "${cmd[0]}" == *run_commandlet.sh* || "${cmd[0]}" == *create_blueprint_batch* ]]; then
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

# 1단계: 블루프린트 생성 및 컴포넌트/디폴트 설정 배치 실행 (에디터 기동 1회)
run_step "1. 블루프린트 일괄 생성 및 설정 배치 처리" \
  "${REPO_ROOT}/tools/ue/create_blueprint_batch.sh" "${REPO_ROOT}/specs/bench/bp_batch_only.json"

TOTAL_END=$(date +%s)
TOTAL_DURATION=$((TOTAL_END - TOTAL_START))

echo ""
echo "=========================================================="
echo "    [BENCHMARK] Batch Blueprint-Only Setup 완료"
echo "    종료 시각: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
echo "    총 소요 시간: ${TOTAL_DURATION}초"
echo "    UnrealEditor-Cmd 기동 횟수: ${EDITOR_LAUNCH_COUNT}회"
echo "=========================================================="
