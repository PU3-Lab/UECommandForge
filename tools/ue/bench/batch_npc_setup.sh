#!/usr/bin/env bash
# 사용법: ./tools/ue/bench/batch_npc_setup.sh
# 목적: C++ 생성 -> 컴파일 -> Batch BP 생성 및 설정을 통해 에디터 기동 횟수를 2회로 줄여 시간 측정

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"

# UE 환경 변수 로드
# shellcheck disable=SC1091
source "${REPO_ROOT}/tools/ue/ue_env.sh"

UE_ROOT_DIR="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.7}"
PROJECT_FILE_ABS="$(cd "$(dirname "${PROJECT_FILE}")" && pwd)/$(basename "${PROJECT_FILE}")"

# 플랫폼별 UBT 빌드 명령어 결정
case "$(uname -s)" in
  Darwin)
    BUILD_SCRIPT="${UE_ROOT_DIR}/Engine/Build/BatchFiles/Mac/Build.sh"
    BUILD_ARGS=(UnrealEditor Mac Development -Project="${PROJECT_FILE_ABS}")
    ;;
  MINGW*|MSYS*|CYGWIN*)
    BUILD_SCRIPT="${UE_ROOT_DIR}/Engine/Build/BatchFiles/Win64/Build.bat"
    BUILD_ARGS=(UnrealEditor Win64 Development -Project="${PROJECT_FILE_ABS}")
    ;;
  *)
    BUILD_SCRIPT="${UE_ROOT_DIR}/Engine/Build/BatchFiles/Linux/Build.sh"
    BUILD_ARGS=(UnrealEditor Linux Development -Project="${PROJECT_FILE_ABS}")
    ;;
esac

# 이전 벤치마크 생성 파일 제거 (Clean)
echo ">>> [CLEAN] 이전 생성된 벤치마크 임시 파일 제거..."
rm -f "${REPO_ROOT}/sample/Plugins/UECommandForge/Source/UECommandForgeRuntime/Public/Generated/Characters/CFBenchCharacter.h"
rm -f "${REPO_ROOT}/sample/Plugins/UECommandForge/Source/UECommandForgeRuntime/Private/Generated/Characters/CFBenchCharacter.cpp"
rm -f "${REPO_ROOT}/sample/Content/AI/Characters/BP_BenchCharacter.uasset"

echo "=========================================================="
echo "    [BENCHMARK] Batch NPC Setup (C++ 포함) 시작"
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

  if [[ "${cmd[0]}" == *UnrealEditor-Cmd* || "${cmd[0]}" == *run_commandlet.sh* || "${cmd[0]}" == *create_blueprint* || "${cmd[0]}" == *generate_cpp_class* ]]; then
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

# 1단계: C++ 캐릭터 클래스 생성
run_step "1. C++ 캐릭터 클래스 생성" \
  "${REPO_ROOT}/tools/ue/generate_cpp_class.sh" "${REPO_ROOT}/specs/bench/cpp_character.json" -Apply

# 2단계: C++ 컴파일 (UBT)
run_step "2. C++ 컴파일 (UBT)" \
  "${BUILD_SCRIPT}" "${BUILD_ARGS[@]}"

# 3단계: C++ 클래스를 상속받아 블루프린트 생성 및 설정 일괄 배치 실행
run_step "3. 블루프린트 생성 및 설정 배치 처리" \
  "${REPO_ROOT}/tools/ue/create_blueprint_batch.sh" "${REPO_ROOT}/specs/bench/bp_batch.json"

TOTAL_END=$(date +%s)
TOTAL_DURATION=$((TOTAL_END - TOTAL_START))

echo ""
echo "=========================================================="
echo "    [BENCHMARK] Batch NPC Setup (C++ 포함) 완료"
echo "    종료 시각: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
echo "    총 소요 시간: ${TOTAL_DURATION}초"
echo "    UnrealEditor-Cmd 기동 횟수: ${EDITOR_LAUNCH_COUNT}회"
echo "=========================================================="
