#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
TMP_DIR="$(mktemp -d)"
trap 'rm -rf "${TMP_DIR}"' EXIT

FAKE_BIN="${TMP_DIR}/bin"
mkdir -p "${FAKE_BIN}"

cat > "${FAKE_BIN}/clang-tidy.exe" <<'FAKE'
#!/usr/bin/env bash
echo "fake clang-tidy"
FAKE
chmod +x "${FAKE_BIN}/clang-tidy.exe"

cat > "${FAKE_BIN}/cppcheck.exe" <<'FAKE'
#!/usr/bin/env bash
echo "fake cppcheck"
FAKE
chmod +x "${FAKE_BIN}/cppcheck.exe"

OUTPUT="$(
  STATIC_ANALYSIS_PLATFORM=windows \
  PATH="${FAKE_BIN}:${PATH}" \
  "${REPO_ROOT}/tools/lint/cpp_static_analysis.sh" --check-tools
)"

echo "${OUTPUT}" | grep -q "clang-tidy: ${FAKE_BIN}/clang-tidy.exe"
echo "${OUTPUT}" | grep -q "cppcheck: ${FAKE_BIN}/cppcheck.exe"
echo "${OUTPUT}" | grep -q "platform: windows"
grep -q 'RUN_CLANG_TIDY' "${REPO_ROOT}/tools/lint/cpp_static_analysis.sh"
grep -q 'CLANG_TIDY_CHECKS' "${REPO_ROOT}/tools/lint/cpp_static_analysis.sh"
