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
grep -q '^install-uecommandforge.sh$' "${ZIP_LIST}"
grep -q '^install-uecommandforge.bat$' "${ZIP_LIST}"
grep -q '^install-uecommandforge.ps1$' "${ZIP_LIST}"
grep -q '^uecommandforge-manifest.json$' "${ZIP_LIST}"
grep -q '^validation-report.json$' "${ZIP_LIST}"

unzip -p "${TOOLS_ZIP}" uecommandforge-manifest.json | jq -e \
  --arg version "${VERSION}" \
  '.version == $version
   and .engine_version == "UE5.7"
   and .release_channel == "local-smoke"
   and (.tool_files | index("tools/ue/run_commandlet.sh"))
   and (.tool_files | index("tools/test/smoke/mid_real_project_flow.sh"))
   and (.spec_files | index("specs/policies/assets.policy.json"))
   and (.install_commands | index("./install-uecommandforge.sh"))
   and (.install_commands | index("install-uecommandforge.bat"))
   and (.install_commands | index("install-uecommandforge.ps1"))
   and (.checksums["tools/ue/run_commandlet.sh"] | type == "string")
   and (.checksums["install-uecommandforge.sh"] | type == "string")
   and (.checksums["install.md"] | type == "string")
   and (.checksums["release-notes.md"] | type == "string")
   and (.checksums["validation-report.json"] | type == "string")
   and (.post_install_checks[] | select(. == "Hello commandlet"))' >/dev/null

unzip -p "${TOOLS_ZIP}" validation-report.json | jq -e \
  --arg version "${VERSION}" \
  '.version == $version
   and .package_type == "tools"
   and .status == "pass"
   and (.checks[] | select(.name == "generated tools package"))' >/dev/null

"${REPO_ROOT}/tools/release/verify_release_package.sh" "${TOOLS_ZIP}" "${CHECKSUMS}"

grep -q "UECommandForge-${VERSION}-Tools.zip" "${CHECKSUMS}"

if "${REPO_ROOT}/tools/release/package_tools.sh" \
  --version '../bad' \
  --out-dir "${WORK_DIR}/bad" >/dev/null 2>&1; then
  echo "unsafe version should fail" >&2
  exit 1
fi

BAD_ZIP_DIR="${WORK_DIR}/bad-zip"
mkdir -p "${BAD_ZIP_DIR}"
touch "${BAD_ZIP_DIR}/bad\\entry"
(
  cd "${BAD_ZIP_DIR}"
  zip -q bad-entry.zip 'bad\entry'
)
if "${REPO_ROOT}/tools/release/verify_release_package.sh" \
  "${BAD_ZIP_DIR}/bad-entry.zip" >/dev/null 2>&1; then
  echo "unsafe zip entry should fail" >&2
  exit 1
fi

BAD_MANIFEST_DIR="${WORK_DIR}/bad-manifest"
rm -rf "${BAD_MANIFEST_DIR}"
mkdir -p "${BAD_MANIFEST_DIR}"
unzip -q "${TOOLS_ZIP}" -d "${BAD_MANIFEST_DIR}/package"
jq 'del(.checksums["install.md"])' \
  "${BAD_MANIFEST_DIR}/package/uecommandforge-manifest.json" \
  > "${BAD_MANIFEST_DIR}/package/uecommandforge-manifest.json.tmp"
mv "${BAD_MANIFEST_DIR}/package/uecommandforge-manifest.json.tmp" \
  "${BAD_MANIFEST_DIR}/package/uecommandforge-manifest.json"
(
  cd "${BAD_MANIFEST_DIR}/package"
  zip -qr "${BAD_MANIFEST_DIR}/missing-checksum.zip" ./*
)
if "${REPO_ROOT}/tools/release/verify_release_package.sh" \
  "${BAD_MANIFEST_DIR}/missing-checksum.zip" >/dev/null 2>&1; then
  echo "manifest missing required checksum should fail" >&2
  exit 1
fi

rm -rf "${BAD_MANIFEST_DIR}/package"
mkdir -p "${BAD_MANIFEST_DIR}/package"
unzip -q "${TOOLS_ZIP}" -d "${BAD_MANIFEST_DIR}/package"
printf 'unexpected\n' > "${BAD_MANIFEST_DIR}/package/extra.sh"
(
  cd "${BAD_MANIFEST_DIR}/package"
  zip -qr "${BAD_MANIFEST_DIR}/unlisted-file.zip" ./*
)
if "${REPO_ROOT}/tools/release/verify_release_package.sh" \
  "${BAD_MANIFEST_DIR}/unlisted-file.zip" >/dev/null 2>&1; then
  echo "unlisted zip file should fail" >&2
  exit 1
fi

rm -rf "${BAD_MANIFEST_DIR}/package"
mkdir -p "${BAD_MANIFEST_DIR}/package"
unzip -q "${TOOLS_ZIP}" -d "${BAD_MANIFEST_DIR}/package"
printf 'unexpected\n' > "${BAD_MANIFEST_DIR}/package/.expected-files.txt"
(
  cd "${BAD_MANIFEST_DIR}/package"
  zip -qr "${BAD_MANIFEST_DIR}/reserved-unlisted-file.zip" .
)
if "${REPO_ROOT}/tools/release/verify_release_package.sh" \
  "${BAD_MANIFEST_DIR}/reserved-unlisted-file.zip" >/dev/null 2>&1; then
  echo "reserved unlisted zip file should fail" >&2
  exit 1
fi

rm -rf "${BAD_MANIFEST_DIR}/package"
mkdir -p "${BAD_MANIFEST_DIR}/package/tools/extra"
unzip -q "${TOOLS_ZIP}" -d "${BAD_MANIFEST_DIR}/package"
printf '{}\n' > "${BAD_MANIFEST_DIR}/package/tools/extra/uecommandforge-manifest.json"
(
  cd "${BAD_MANIFEST_DIR}/package"
  zip -qr "${BAD_MANIFEST_DIR}/nested-manifest-file.zip" ./*
)
if "${REPO_ROOT}/tools/release/verify_release_package.sh" \
  "${BAD_MANIFEST_DIR}/nested-manifest-file.zip" >/dev/null 2>&1; then
  echo "nested manifest-named unlisted zip file should fail" >&2
  exit 1
fi

rm -rf "${BAD_MANIFEST_DIR}/package"
mkdir -p "${BAD_MANIFEST_DIR}/package"
printf 'x' > "${BAD_MANIFEST_DIR}/package/install.md"
printf 'x' > "${BAD_MANIFEST_DIR}/package/release-notes.md"
printf 'x' > "${BAD_MANIFEST_DIR}/package/validation-report.json"
jq -n '{
  version: "bad",
  engine_version: "UE5.7",
  release_channel: "local-smoke",
  plugin_files: [".."],
  tool_files: [],
  spec_files: [],
  checksums: {"..": "0", "install.md": "0", "release-notes.md": "0", "validation-report.json": "0"},
  install_commands: [],
  post_install_checks: ["Hello commandlet"]
}' > "${BAD_MANIFEST_DIR}/package/uecommandforge-manifest.json"
(
  cd "${BAD_MANIFEST_DIR}/package"
  zip -qr "${BAD_MANIFEST_DIR}/unsafe-manifest-path.zip" ./*
)
if "${REPO_ROOT}/tools/release/verify_release_package.sh" \
  "${BAD_MANIFEST_DIR}/unsafe-manifest-path.zip" >/dev/null 2>&1; then
  echo "unsafe manifest path should fail" >&2
  exit 1
fi

BAD_CHECKSUMS="${WORK_DIR}/bad-checksums.txt"
printf '%064d  %s\n' 0 "$(basename "${TOOLS_ZIP}")" > "${BAD_CHECKSUMS}"
if "${REPO_ROOT}/tools/release/verify_release_package.sh" \
  "${TOOLS_ZIP}" "${BAD_CHECKSUMS}" >/dev/null 2>&1; then
  echo "bad checksum should fail" >&2
  exit 1
fi

SIDE_FILE="${WORK_DIR}/side.txt"
SIDE_CHECKSUMS="${WORK_DIR}/side-checksums.txt"
printf 'side\n' > "${SIDE_FILE}"
"${REPO_ROOT}/tools/release/write_checksums.sh" "${SIDE_FILE}" > "${SIDE_CHECKSUMS}"
if "${REPO_ROOT}/tools/release/verify_release_package.sh" \
  "${TOOLS_ZIP}" "${SIDE_CHECKSUMS}" >/dev/null 2>&1; then
  echo "checksum for a different file should fail" >&2
  exit 1
fi
