#!/usr/bin/env bash
# 사용법: ./tools/test/smoke/cpp_generation_batch.sh
# 목적: C++ 배치 생성 commandlet 및 래퍼가 단일 Unreal Editor 실행으로 복수 클래스를 생성하는지 확인

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
SAMPLE_DIR="${REPO_ROOT}/sample"

# 기존 테스트 정리
CLEANUP_FILES=(
  "${SAMPLE_DIR}/Plugins/UECommandForge/Source/UECommandForgeRuntime/Public/Generated/SmokeBatchActorA.h"
  "${SAMPLE_DIR}/Plugins/UECommandForge/Source/UECommandForgeRuntime/Private/Generated/SmokeBatchActorA.cpp"
  "${SAMPLE_DIR}/Plugins/UECommandForge/Source/UECommandForgeRuntime/Public/Generated/SmokeBatchActorB.h"
  "${SAMPLE_DIR}/Plugins/UECommandForge/Source/UECommandForgeRuntime/Private/Generated/SmokeBatchActorB.cpp"
)

cleanup() {
  echo ">>> [CLEANUP] 이전 생성된 테스트 파일 정리..."
  for file in "${CLEANUP_FILES[@]}"; do
    rm -f "$file"
  done
  rm -f "${SAMPLE_DIR}/Saved/UECommandForge/Reports/GenerateCppClassBatch_"*.json
}

cleanup

REPORT_DIR="${SAMPLE_DIR}/Saved/UECommandForge/Reports"
mkdir -p "${REPORT_DIR}"

SPEC_FILE="${REPORT_DIR}/smoke_cpp_class_batch.json"

# 1. 배치 spec JSON 생성
echo ">>> [SETUP] C++ 클래스 배치 생성 스펙 작성..."
cat << 'EOF' > "${SPEC_FILE}"
{
  "version": "1",
  "kind": "cpp_class_batch",
  "transaction_id": "tx-smoke-batch-cpp",
  "classes": [
    {
      "name": "SmokeBatchActorA",
      "type": "Actor",
      "module": "UECommandForgeRuntime",
      "output_path": "Generated",
      "base_class": "AActor",
      "blueprint_type": true,
      "tick": false
    },
    {
      "name": "SmokeBatchActorB",
      "type": "Actor",
      "module": "UECommandForgeRuntime",
      "output_path": "Generated",
      "base_class": "AActor",
      "blueprint_type": true,
      "tick": false
    }
  ],
  "includes": [
    "CoreMinimal.h"
  ],
  "properties": [
    {
      "name": "BaseValue",
      "type": "float",
      "default": "1.0f",
      "metadata": {
        "EditAnywhere": true,
        "Category": "Smoke"
      }
    }
  ],
  "build_dependencies": {
    "public": [
      "Engine"
    ]
  }
}
EOF

# 2. 배치 생성 래퍼 구동
echo ">>> [RUN] generate_cpp_class_batch.sh 실행..."
"${REPO_ROOT}/tools/ue/generate_cpp_class_batch.sh" "${SPEC_FILE}" -Apply >/dev/null

# 3. 리포트 및 생성 파일 검증
echo ">>> [VERIFY] 결과 검증..."
OUTPUT_REPORT="$(ls -t "${SAMPLE_DIR}/Saved/UECommandForge/Reports/GenerateCppClassBatch_"*.json | head -1)"

if [ ! -f "${OUTPUT_REPORT}" ]; then
  echo "FAIL: 리포트 파일이 생성되지 않았습니다." >&2
  exit 1
fi

echo "  사용된 리포트: ${OUTPUT_REPORT}"

OK_STATUS=$(jq -r '.ok // empty' "${OUTPUT_REPORT}")
APPLIED_STATUS=$(jq -r '.applied // empty' "${OUTPUT_REPORT}")
CHANGED_FILES_COUNT=$(jq '.changed_files | length' "${OUTPUT_REPORT}")

echo "  ok: ${OK_STATUS}"
echo "  applied: ${APPLIED_STATUS}"
echo "  changed files count: ${CHANGED_FILES_COUNT}"

if [ "${OK_STATUS}" != "true" ]; then
  echo "FAIL: 리포트의 ok 상태가 true가 아닙니다." >&2
  exit 1
fi

if [ "${APPLIED_STATUS}" != "true" ]; then
  echo "FAIL: 리포트의 applied 상태가 true가 아닙니다." >&2
  exit 1
fi

# 2개 클래스의 헤더/소스 총 4개 파일 생성 여부 체크
if [ "${CHANGED_FILES_COUNT}" -ne 4 ]; then
  echo "FAIL: 변경된 파일 개수가 4개가 아닙니다 (실제: ${CHANGED_FILES_COUNT})." >&2
  exit 1
fi

for file in "${CLEANUP_FILES[@]}"; do
  if [ ! -f "$file" ]; then
    echo "FAIL: 대상 파일이 생성되지 않았습니다: $file" >&2
    exit 1
  fi
  echo "  생성 확인: $(basename "$file")"
done

echo "[cpp-generation-batch] ok: C++ 배치 클래스 생성 검증 통과"

# 정리 진행
cleanup
