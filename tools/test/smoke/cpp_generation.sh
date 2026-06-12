#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
SAMPLE_DIR="${REPO_ROOT}/sample"
SPEC_PATH="${REPO_ROOT}/specs/examples/cpp_health_component.json"
WORK_DIR="${SAMPLE_DIR}/Saved/UECommandForge/Reports/CppGenerationSmoke"
HEADER_PATH="${SAMPLE_DIR}/Plugins/UECommandForge/Source/UECommandForgeRuntime/Public/Generated/Components/UCFHealthComponent.h"
SOURCE_PATH="${SAMPLE_DIR}/Plugins/UECommandForge/Source/UECommandForgeRuntime/Private/Generated/Components/UCFHealthComponent.cpp"
BUILD_LOG="${WORK_DIR}/build_plugin.log"

export UE_COMMANDLET_TIMEOUT="${UE_COMMANDLET_TIMEOUT:-90}"

mkdir -p "${WORK_DIR}"
rm -f "${SAMPLE_DIR}/Saved/UECommandForge/Reports/GenerateCppClass_"*.json
rm -f "${HEADER_PATH}" "${SOURCE_PATH}"

cleanup_generated_class() {
  rm -f "${HEADER_PATH}" "${SOURCE_PATH}"
  rmdir "${SAMPLE_DIR}/Plugins/UECommandForge/Source/UECommandForgeRuntime/Public/Generated/Components" 2>/dev/null || true
  rmdir "${SAMPLE_DIR}/Plugins/UECommandForge/Source/UECommandForgeRuntime/Public/Generated" 2>/dev/null || true
  rmdir "${SAMPLE_DIR}/Plugins/UECommandForge/Source/UECommandForgeRuntime/Private/Generated/Components" 2>/dev/null || true
  rmdir "${SAMPLE_DIR}/Plugins/UECommandForge/Source/UECommandForgeRuntime/Private/Generated" 2>/dev/null || true
}
trap cleanup_generated_class EXIT

"${REPO_ROOT}/tools/ue/generate_cpp_class.sh" "${SPEC_PATH}" >/dev/null
REPORT="$(ls -t "${SAMPLE_DIR}/Saved/UECommandForge/Reports/GenerateCppClass_"*.json | head -1)"

test -f "${REPORT}"
jq -e '.ok == true' "${REPORT}" >/dev/null
jq -e '.commandlet == "GenerateCppClass"' "${REPORT}" >/dev/null
jq -e '.dry_run == true' "${REPORT}" >/dev/null
jq -e '.applied == false' "${REPORT}" >/dev/null
jq -e '.validation.class_symbol == "UCFHealthComponent"' "${REPORT}" >/dev/null
jq -e '.validation.header_preview | contains("UCLASS(BlueprintType")' "${REPORT}" >/dev/null
jq -e '.validation.header_sha1 | length == 40' "${REPORT}" >/dev/null
jq -e '(.changed_files | length) == 0' "${REPORT}" >/dev/null

rm -f "${SAMPLE_DIR}/Saved/UECommandForge/Reports/GenerateCppClass_"*.json

"${REPO_ROOT}/tools/ue/generate_cpp_class.sh" "${SPEC_PATH}" -Apply >/dev/null
APPLY_REPORT="$(ls -t "${SAMPLE_DIR}/Saved/UECommandForge/Reports/GenerateCppClass_"*.json | head -1)"

test -f "${APPLY_REPORT}"
jq -e '.ok == true' "${APPLY_REPORT}" >/dev/null
jq -e '.dry_run == false' "${APPLY_REPORT}" >/dev/null
jq -e '.applied == true' "${APPLY_REPORT}" >/dev/null
jq -e '(.changed_files | length) == 2' "${APPLY_REPORT}" >/dev/null
test -f "${HEADER_PATH}"
test -f "${SOURCE_PATH}"
grep -q 'UCLASS(BlueprintType' "${HEADER_PATH}"
grep -q 'UCFHealthComponent::ApplyDamage' "${SOURCE_PATH}"

if ! "${REPO_ROOT}/tools/ue/build_plugin.sh" >"${BUILD_LOG}" 2>&1; then
  cat "${BUILD_LOG}" >&2
  exit 1
fi
