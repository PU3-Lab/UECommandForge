#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
WORK_DIR="${REPO_ROOT}/sample/Saved/CodexReports/ReleasePackageTools"
VERSION="0.8.0-test"

rm -rf "${WORK_DIR}"
mkdir -p "${WORK_DIR}"

"${REPO_ROOT}/tools/release/package_tools.sh" \
  --version "${VERSION}" \
  --channel local-smoke \
  --out-dir "${WORK_DIR}"

TOOLS_ZIP="${WORK_DIR}/UECommandForge-${VERSION}-Tools.zip"
CHECKSUMS="${WORK_DIR}/checksums.txt"
ZIP_LIST="${WORK_DIR}/zip-files.txt"

test -f "${TOOLS_ZIP}"
test -f "${CHECKSUMS}"

unzip -Z1 "${TOOLS_ZIP}" > "${ZIP_LIST}"
grep -q '^tools/ue/run_commandlet.sh$' "${ZIP_LIST}"
grep -q '^tools/test/smoke/mid_real_project_flow.sh$' "${ZIP_LIST}"
grep -q '^tools/release/package_tools.sh$' "${ZIP_LIST}"
grep -q '^specs/policies/assets.policy.json$' "${ZIP_LIST}"
grep -q '^uecommandforge-manifest.json$' "${ZIP_LIST}"

unzip -p "${TOOLS_ZIP}" uecommandforge-manifest.json | jq -e \
  --arg version "${VERSION}" \
  '.version == $version
   and .engine_version == "UE5.7"
   and .release_channel == "local-smoke"
   and (.tool_files | index("tools/ue/run_commandlet.sh"))
   and (.tool_files | index("tools/test/smoke/mid_real_project_flow.sh"))
   and (.spec_files | index("specs/policies/assets.policy.json"))
   and (.install_commands[] | select(. == "install-uecommandforge.sh --project <path-to-uproject>"))
   and (.post_install_checks[] | select(. == "Hello commandlet"))' >/dev/null

"${REPO_ROOT}/tools/release/verify_release_package.sh" "${TOOLS_ZIP}"

grep -q "UECommandForge-${VERSION}-Tools.zip" "${CHECKSUMS}"

if "${REPO_ROOT}/tools/release/package_tools.sh" \
  --version '../bad' \
  --out-dir "${WORK_DIR}/bad" >/dev/null 2>&1; then
  echo "unsafe version should fail" >&2
  exit 1
fi
