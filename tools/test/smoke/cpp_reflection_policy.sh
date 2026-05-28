#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
SAMPLE_DIR="${REPO_ROOT}/sample"
WORK_DIR="${SAMPLE_DIR}/Saved/CodexReports/ReflectionSmoke"
HEADER_PATH="${WORK_DIR}/BrokenReflectionComponent.h"
POLICY_PATH="${WORK_DIR}/cpp_reflection_policy.json"

export UE_COMMANDLET_TIMEOUT="${UE_COMMANDLET_TIMEOUT:-90}"

mkdir -p "${WORK_DIR}"
rm -f "${SAMPLE_DIR}/Saved/CodexReports/ValidateCppReflection_"*.json

cat > "${HEADER_PATH}" <<'HEADER'
#pragma once

#include "Components/ActorComponent.h"
#include "BrokenReflectionComponent.generated.h"

UCLASS(BlueprintType)
class UBrokenReflectionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float RuntimeHealth = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Components")
    TObjectPtr<USceneComponent> AnchorComponent;

    UPROPERTY(Config, EditDefaultsOnly, Category="Config")
    float ConfiguredScale = 1.0f;

    UFUNCTION(BlueprintCallable)
    void ApplyDamage(float Amount);

    UPROPERTY(BlueprintReadOnly, Category="Broken")
};
HEADER

cat > "${POLICY_PATH}" <<JSON
{
  "version": "1",
  "kind": "cpp_reflection_policy",
  "files": [
    {
      "path": "${HEADER_PATH}",
      "role": "ActorComponent"
    }
  ]
}
JSON

set +e
"${REPO_ROOT}/tools/ue/validate_cpp_reflection.sh" "${POLICY_PATH}" >/dev/null
EXIT_CODE=$?
set -e

test "${EXIT_CODE}" -eq 4
REPORT="$(ls -t "${SAMPLE_DIR}/Saved/CodexReports/ValidateCppReflection_"*.json | head -1)"

test -f "${REPORT}"
jq -e '.ok == false' "${REPORT}" >/dev/null
jq -e '.commandlet == "ValidateCppReflection"' "${REPORT}" >/dev/null
jq -e '.validation.checked_file_count == "1"' "${REPORT}" >/dev/null
jq -e '.issues[] | select(.code == "CPP_REFLECTION_PROPERTY_CATEGORY_MISSING")' "${REPORT}" >/dev/null
jq -e '.issues[] | select(.code == "CPP_REFLECTION_FUNCTION_CATEGORY_MISSING")' "${REPORT}" >/dev/null
jq -e '.issues[] | select(.code == "CPP_REFLECTION_RUNTIME_MUTABLE_EDITABLE")' "${REPORT}" >/dev/null
jq -e '.issues[] | select(.code == "CPP_REFLECTION_COMPONENT_NOT_VISIBLEANYWHERE")' "${REPORT}" >/dev/null
jq -e '.issues[] | select(.code == "CPP_REFLECTION_CONFIG_WITHOUT_CONFIG_CLASS")' "${REPORT}" >/dev/null
jq -e '.issues[] | select(.code == "CPP_REFLECTION_MACRO_WITHOUT_DECLARATION")' "${REPORT}" >/dev/null
