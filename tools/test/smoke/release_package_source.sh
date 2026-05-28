#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
WORK_DIR="${REPO_ROOT}/sample/Saved/CodexReports/ReleasePackageSource"
VERSION="$(jq -r '.VersionName' "${REPO_ROOT}/sample/Plugins/UECommandForge/UECommandForge.uplugin")"

rm -rf "${WORK_DIR}"
mkdir -p "${WORK_DIR}"

"${REPO_ROOT}/tools/release/package_source.sh" \
  --version "${VERSION}" \
  --channel local-smoke \
  --out-dir "${WORK_DIR}"

SOURCE_ZIP="${WORK_DIR}/UECommandForge-${VERSION}-Source.zip"
CHECKSUMS="${WORK_DIR}/checksums.txt"
ZIP_LIST="${WORK_DIR}/zip-files.txt"

test -f "${SOURCE_ZIP}"
test -f "${CHECKSUMS}"

unzip -Z1 "${SOURCE_ZIP}" > "${ZIP_LIST}"
grep -q '^README.md$' "${ZIP_LIST}"
grep -q '^sample/UECommandForgeSample.uproject$' "${ZIP_LIST}"
grep -q '^sample/Plugins/UECommandForge/UECommandForge.uplugin$' "${ZIP_LIST}"
grep -q '^sample/Plugins/UECommandForge/Source/UECommandForgeRuntime/UECommandForgeRuntime.Build.cs$' "${ZIP_LIST}"
grep -q '^tools/release/package_source.sh$' "${ZIP_LIST}"
grep -q '^specs/policies/assets.policy.json$' "${ZIP_LIST}"
grep -q '^uecommandforge-manifest.json$' "${ZIP_LIST}"
grep -q '^install.md$' "${ZIP_LIST}"
grep -q '^release-notes.md$' "${ZIP_LIST}"
grep -q '^validation-report.json$' "${ZIP_LIST}"

if grep -q '^\.git/' "${ZIP_LIST}"; then
  echo "source package must not include .git" >&2
  exit 1
fi
if grep -q '^sample/Saved/' "${ZIP_LIST}"; then
  echo "source package must not include sample/Saved" >&2
  exit 1
fi
if grep -q '^sample/Plugins/UECommandForge/Binaries/' "${ZIP_LIST}"; then
  echo "source package must not include plugin binaries" >&2
  exit 1
fi
if grep -q '^sample/Plugins/UECommandForge/Intermediate/' "${ZIP_LIST}"; then
  echo "source package must not include plugin Intermediate" >&2
  exit 1
fi
if grep -q '^sample/Plugins/UECommandForge/Saved/' "${ZIP_LIST}"; then
  echo "source package must not include plugin Saved" >&2
  exit 1
fi
if grep -q '^sample/Plugins/UECommandForge/DerivedDataCache/' "${ZIP_LIST}"; then
  echo "source package must not include plugin DerivedDataCache" >&2
  exit 1
fi
if grep -q '^docs/memory/' "${ZIP_LIST}"; then
  echo "source package must not include internal memory docs" >&2
  exit 1
fi

unzip -p "${SOURCE_ZIP}" uecommandforge-manifest.json | jq -e \
  --arg version "${VERSION}" \
  '.version == $version
   and .engine_version == "UE5.7"
   and .release_channel == "local-smoke"
   and (.plugin_files | index("README.md"))
   and (.plugin_files | index("sample/Plugins/UECommandForge/UECommandForge.uplugin"))
   and (.tool_files | index("tools/release/package_source.sh"))
   and (.spec_files | index("specs/policies/assets.policy.json"))
   and (.install_commands | length == 0)
   and (.checksums["validation-report.json"] | type == "string")
   and (.post_install_checks[] | select(. == "source package file list"))' >/dev/null

unzip -p "${SOURCE_ZIP}" validation-report.json | jq -e \
  --arg version "${VERSION}" \
  '.version == $version
   and .package_type == "source"
   and .status == "pass"
   and (.checks[] | select(.name == "generated source package"))' >/dev/null

"${REPO_ROOT}/tools/release/verify_release_package.sh" "${SOURCE_ZIP}" "${CHECKSUMS}"

grep -q "UECommandForge-${VERSION}-Source.zip" "${CHECKSUMS}"

BAD_SOURCE_DIR="${WORK_DIR}/bad-source"
mkdir -p "${BAD_SOURCE_DIR}"
unzip -q "${SOURCE_ZIP}" -d "${BAD_SOURCE_DIR}/package"
mkdir -p "${BAD_SOURCE_DIR}/package/sample/Saved"
printf 'unexpected\n' > "${BAD_SOURCE_DIR}/package/sample/Saved/leak.txt"
(
  cd "${BAD_SOURCE_DIR}/package"
  zip -qr "${BAD_SOURCE_DIR}/saved-leak.zip" ./*
)
if "${REPO_ROOT}/tools/release/verify_release_package.sh" \
  "${BAD_SOURCE_DIR}/saved-leak.zip" >/dev/null 2>&1; then
  echo "unlisted source package file should fail" >&2
  exit 1
fi

if "${REPO_ROOT}/tools/release/package_source.sh" \
  --version '0.0.0-mismatch' \
  --out-dir "${WORK_DIR}/bad-version" >/dev/null 2>&1; then
  echo "mismatched source package version should fail" >&2
  exit 1
fi
