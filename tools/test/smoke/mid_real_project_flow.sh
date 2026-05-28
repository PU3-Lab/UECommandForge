#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
SAMPLE_DIR="${REPO_ROOT}/sample"
PROJECT_FILE="${SAMPLE_DIR}/UECommandForgeSample.uproject"
WORK_DIR="${SAMPLE_DIR}/Saved/CodexReports/MidRealProject"
CODEX_HOME="${WORK_DIR}/codex-home"
CODEX_APP="${CODEX_HOME}/UECommandForge"
REPORT_MD="${WORK_DIR}/mid_real_project_flow.md"
UHT_LOG_PATH="${WORK_DIR}/uht_errors.log"
UHT_MARKDOWN_PATH="${WORK_DIR}/uht_errors.md"
REFLECTION_HEADER_PATH="${WORK_DIR}/BrokenReflectionComponent.h"
REFLECTION_POLICY_PATH="${WORK_DIR}/cpp_reflection_policy.json"

export UE_COMMANDLET_TIMEOUT="${UE_COMMANDLET_TIMEOUT:-90}"
export UECF_PROJECT_FILE="${PROJECT_FILE}"

case "${CODEX_HOME}" in
  "${WORK_DIR}/codex-home") ;;
  *)
    echo "[mid_real_project_flow] unsafe CODEX_HOME: ${CODEX_HOME}" >&2
    exit 2
    ;;
esac

rm -rf "${CODEX_HOME}"
mkdir -p "${CODEX_APP}" "${WORK_DIR}"
cp -R "${REPO_ROOT}/tools" "${CODEX_APP}/tools"
cp -R "${REPO_ROOT}/specs" "${CODEX_APP}/specs"

cat > "${REPORT_MD}" <<REPORT
# Phase 8 Mid Real Project Flow

- Project: \`${PROJECT_FILE}\`
- Codex tools root: \`${CODEX_APP}\`

REPORT

record_pass() {
  printf -- "- PASS: %s\n" "$1" >> "${REPORT_MD}"
}

LAST_REPORT=""

extract_report_path() {
  awk '/Saved\/CodexReports\/.*\.json$/ { line = $0 } END { print line }'
}

run_expect_ok() {
  local label="$1"
  shift
  local output
  output="$("$@")"
  LAST_REPORT="$(printf '%s\n' "${output}" | extract_report_path)"
  test -f "${LAST_REPORT}"
  record_pass "${label}"
}

run_expect_exit() {
  local expected="$1"
  local label="$2"
  shift 2
  local output
  set +e
  output="$("$@")"
  local exit_code=$?
  set -e
  LAST_REPORT="$(printf '%s\n' "${output}" | extract_report_path)"
  test "${exit_code}" -eq "${expected}"
  test -f "${LAST_REPORT}"
  record_pass "${label}"
}

cat > "${UHT_LOG_PATH}" <<'LOG'
LogInit: Running UnrealHeaderTool
/Project/Plugins/UECommandForge/Source/CodexBad/Public/BadComponent.h(17): Error: Unrecognized type 'FInventoryRow' - type must be a UCLASS, USTRUCT or UENUM
/Project/Plugins/UECommandForge/Source/CodexBad/Public/BadComponent.h(31): Error: BlueprintReadWrite should not be used on private members
LOG

cat > "${REFLECTION_HEADER_PATH}" <<'HEADER'
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

    UFUNCTION(BlueprintCallable)
    void ApplyDamage(float Amount);
};
HEADER

cat > "${REFLECTION_POLICY_PATH}" <<JSON
{
  "version": "1",
  "kind": "cpp_reflection_policy",
  "files": [
    {
      "path": "${REFLECTION_HEADER_PATH}",
      "role": "ActorComponent"
    }
  ]
}
JSON

run_expect_ok "Hello commandlet from Codex tools root" \
  "${CODEX_APP}/tools/ue/hello.sh"
HELLO_REPORT="${LAST_REPORT}"
jq -e '.ok == true and .commandlet == "Hello"' "${HELLO_REPORT}" >/dev/null

run_expect_ok "Asset snapshot from Codex tools root" \
  "${CODEX_APP}/tools/ue/snapshot_assets.sh" -RootPaths=/Game/Tests
SNAPSHOT_REPORT="${LAST_REPORT}"
jq -e '.ok == true and .commandlet == "AssetSnapshot"' "${SNAPSHOT_REPORT}" >/dev/null
jq -e '.validation.asset_count == "11"' "${SNAPSHOT_REPORT}" >/dev/null

run_expect_ok "Asset policy from Codex specs root" \
  "${CODEX_APP}/tools/ue/validate_asset_rules.sh" \
  "${CODEX_APP}/specs/policies/assets.policy.json" \
  -RootPaths=/Game/Tests
ASSET_POLICY_REPORT="${LAST_REPORT}"
jq -e '.ok == true and .validation.issue_count == "0"' "${ASSET_POLICY_REPORT}" >/dev/null

run_expect_ok "Build.cs policy from Codex specs root" \
  "${CODEX_APP}/tools/ue/validate_buildcs.sh" \
  "${CODEX_APP}/specs/policies/buildcs.policy.json"
BUILDCS_REPORT="${LAST_REPORT}"
jq -e '.ok == true and .validation.issue_count == "0"' "${BUILDCS_REPORT}" >/dev/null

run_expect_ok "C++ generation dry-run from Codex specs root" \
  "${CODEX_APP}/tools/ue/generate_cpp_class.sh" \
  "${CODEX_APP}/specs/examples/cpp_health_component.json"
CPP_REPORT="${LAST_REPORT}"
jq -e '.ok == true and .dry_run == true and .applied == false' "${CPP_REPORT}" >/dev/null

run_expect_exit 4 "Reflection policy violation report from project fixture" \
  "${CODEX_APP}/tools/ue/validate_cpp_reflection.sh" "${REFLECTION_POLICY_PATH}"
REFLECTION_REPORT="${LAST_REPORT}"
jq -e '.ok == false' "${REFLECTION_REPORT}" >/dev/null
jq -e '.issues[] | select(.code == "CPP_REFLECTION_PROPERTY_CATEGORY_MISSING")' "${REFLECTION_REPORT}" >/dev/null

run_expect_exit 4 "UHT log analysis report and Markdown" \
  "${CODEX_APP}/tools/ue/analyze_uht_log.sh" "${UHT_LOG_PATH}" -Markdown="${UHT_MARKDOWN_PATH}"
UHT_REPORT="${LAST_REPORT}"
jq -e '.ok == false and .validation.error_count == "2"' "${UHT_REPORT}" >/dev/null
grep -q 'AnalyzeUhtLog Report' "${UHT_MARKDOWN_PATH}"

record_pass "Mid real project flow report written"
