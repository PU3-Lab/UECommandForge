#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
SAMPLE_DIR="${REPO_ROOT}/sample"
WORK_DIR="${SAMPLE_DIR}/Saved/UECommandForge/Reports/ConfigDiffSmoke"
ALLOWLIST_PATH="${WORK_DIR}/diff_allowlist.json"

export UE_COMMANDLET_TIMEOUT="${UE_COMMANDLET_TIMEOUT:-90}"

mkdir -p "${WORK_DIR}"
rm -f "${SAMPLE_DIR}/Saved/UECommandForge/Reports/DiffPlatformConfig_"*.json

echo "[smoke-config] 1. Allow-all config diff test (should pass with ok: true)"
cat > "${ALLOWLIST_PATH}" <<JSON
[
  { "section": "*", "key": "*" }
]
JSON

set +e
"${REPO_ROOT}/tools/ue/diff_platform_config.sh" "${ALLOWLIST_PATH}" \
  -Project="${SAMPLE_DIR}/UECommandForgeSample.uproject" -Platforms=Windows,Mac -Categories=Engine >/dev/null
EXIT_CODE=$?
set -e

if [ "${EXIT_CODE}" -ne 0 ]; then
  echo "[smoke-config] FAIL: Allow-all list returned non-zero exit code: ${EXIT_CODE}" >&2
  exit 1
fi

REPORT="$(ls -t "${SAMPLE_DIR}/Saved/UECommandForge/Reports/DiffPlatformConfig_"*.json | head -1)"
test -f "${REPORT}"
jq -e '.ok == true' "${REPORT}" >/dev/null
jq -e '.validation.diff_count == "0"' "${REPORT}" >/dev/null

echo "[smoke-config] 2. Empty allowlist test (should return ValidationFailed(4) if differences exist)"
cat > "${ALLOWLIST_PATH}" <<JSON
[]
JSON

rm -f "${SAMPLE_DIR}/Saved/UECommandForge/Reports/DiffPlatformConfig_"*.json

set +e
"${REPO_ROOT}/tools/ue/diff_platform_config.sh" "${ALLOWLIST_PATH}" \
  -Project="${SAMPLE_DIR}/UECommandForgeSample.uproject" -Platforms=Windows,Mac -Categories=Engine >/dev/null
EXIT_CODE=$?
set -e

# ValidationFailed = 4 or 0 (if project has absolutely 0 differences between Windows and Mac)
if [ "${EXIT_CODE}" -ne 4 ] && [ "${EXIT_CODE}" -ne 0 ]; then
  echo "[smoke-config] FAIL: Empty allowlist returned unexpected exit code: ${EXIT_CODE}" >&2
  exit 1
fi

REPORT="$(ls -t "${SAMPLE_DIR}/Saved/UECommandForge/Reports/DiffPlatformConfig_"*.json | head -1)"
test -f "${REPORT}"
echo "[smoke-config] Report verified. ok state is: $(jq -r '.ok' "${REPORT}")"

echo "[smoke-config] DiffPlatformConfig smoke test passed successfully."
