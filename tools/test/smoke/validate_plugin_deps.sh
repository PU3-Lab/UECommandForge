#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
SAMPLE_DIR="${REPO_ROOT}/sample"
WORK_DIR="${SAMPLE_DIR}/Saved/UECommandForge/Reports/PluginDepsSmoke"
POLICY_PATH="${WORK_DIR}/plugin_policy.json"

export UE_COMMANDLET_TIMEOUT="${UE_COMMANDLET_TIMEOUT:-90}"

mkdir -p "${WORK_DIR}"
rm -f "${SAMPLE_DIR}/Saved/UECommandForge/Reports/ValidatePluginDependencies_"*.json

echo "[smoke] 1. Valid policy test (EnhancedInput required, EditorScriptingUtilities forbidden but shipping logic filters it)"
cat > "${POLICY_PATH}" <<JSON
{
  "version": "1",
  "kind": "plugin_dependency_policy",
  "required": ["EnhancedInput"],
  "forbiddenInShipping": ["EditorScriptingUtilities", "LevelSequenceEditor", "Cascade"],
  "optional": [],
  "allowedEditorOnly": []
}
JSON

set +e
"${REPO_ROOT}/tools/ue/validate_plugin_deps.sh" "${POLICY_PATH}" \
  -Project="${SAMPLE_DIR}/UECommandForgeSample.uproject" -Configuration=Shipping >/dev/null
EXIT_CODE=$?
set -e

if [ "${EXIT_CODE}" -ne 0 ]; then
  echo "[smoke] FAIL: Valid policy returned non-zero exit code: ${EXIT_CODE}" >&2
  exit 1
fi

REPORT="$(ls -t "${SAMPLE_DIR}/Saved/UECommandForge/Reports/ValidatePluginDependencies_"*.json | head -1)"
test -f "${REPORT}"
jq -e '.ok == true' "${REPORT}" >/dev/null

echo "[smoke] 2. Missing required plugin test (NonExistentPluginNameForTest)"
cat > "${POLICY_PATH}" <<JSON
{
  "version": "1",
  "kind": "plugin_dependency_policy",
  "required": ["NonExistentPluginNameForTest"],
  "forbiddenInShipping": [],
  "optional": [],
  "allowedEditorOnly": []
}
JSON

rm -f "${SAMPLE_DIR}/Saved/UECommandForge/Reports/ValidatePluginDependencies_"*.json

set +e
"${REPO_ROOT}/tools/ue/validate_plugin_deps.sh" "${POLICY_PATH}" \
  -Project="${SAMPLE_DIR}/UECommandForgeSample.uproject" -Configuration=Shipping >/dev/null
EXIT_CODE=$?
set -e

if [ "${EXIT_CODE}" -ne 4 ]; then
  echo "[smoke] FAIL: Missing plugin test did not return ValidationFailed(4): ${EXIT_CODE}" >&2
  exit 1
fi

REPORT="$(ls -t "${SAMPLE_DIR}/Saved/UECommandForge/Reports/ValidatePluginDependencies_"*.json | head -1)"
test -f "${REPORT}"
jq -e '.ok == false' "${REPORT}" >/dev/null
jq -e '.issues[] | select(.code == "PLUGIN_REQUIRED_MISSING")' "${REPORT}" >/dev/null

echo "[smoke] ValidatePluginDependencies smoke test passed successfully."
