#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
# shellcheck disable=SC1091
source "${REPO_ROOT}/tools/release/common.sh"
WORK_DIR="${REPO_ROOT}/sample/Saved/UECommandForge/Reports/ReleasePackageSource"
VERSION="$(jq -r '.VersionName' "${REPO_ROOT}/sample/Plugins/UECommandForge/UECommandForge.uplugin")"
SOURCE_REPARSE_LINK="${REPO_ROOT}/tools/test/smoke/.source_package_reparse_fixture"
OUTPUT_REPARSE_LINK=""

remove_reparse_fixture() {
  local path="$1"
  if [ -z "${path}" ]; then
    return 0
  fi

  if uecf_is_windows_bash && command -v cygpath >/dev/null 2>&1 && command -v powershell.exe >/dev/null 2>&1; then
    local windows_path
    windows_path="$(cygpath -w "${path}")"
    powershell.exe -NoProfile -ExecutionPolicy Bypass -Command \
      '& {
        param([string]$Path)
        $ErrorActionPreference = "Stop"
        if (Test-Path -LiteralPath $Path) {
          $Item = Get-Item -LiteralPath $Path -Force
          if (($Item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
            $Item.Delete()
          } else {
            Remove-Item -LiteralPath $Path -Recurse -Force
          }
        }
      }' \
      "${windows_path}" >/dev/null
  else
    rm -rf "${path}"
  fi
}

create_reparse_fixture() {
  local link="$1"
  local target="$2"
  remove_reparse_fixture "${link}"

  if uecf_is_windows_bash; then
    local windows_link
    local windows_target
    windows_link="$(cygpath -w "${link}")"
    windows_target="$(cygpath -w "${target}")"
    powershell.exe -NoProfile -ExecutionPolicy Bypass -Command \
      '& {
        param([string]$Link, [string]$Target)
        $ErrorActionPreference = "Stop"
        New-Item -ItemType Junction -Path $Link -Target $Target -Force | Out-Null
      }' \
      "${windows_link}" "${windows_target}" >/dev/null
  else
    ln -s "${target}" "${link}"
  fi

  uecf_path_is_reparse_point "${link}"
}

cleanup() {
  remove_reparse_fixture "${SOURCE_REPARSE_LINK}"
  remove_reparse_fixture "${OUTPUT_REPARSE_LINK}"
}
trap cleanup EXIT

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
grep -q '^tools/release/install_local.sh$' "${ZIP_LIST}"
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
if grep -q '^docs/superpowers/plans/' "${ZIP_LIST}"; then
  echo "source package must not include internal planning docs" >&2
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

if unzip -p "${SOURCE_ZIP}" install.md | grep -q 'installer scripts are not shipped yet'; then
  echo "source package install guide must not claim installer scripts are missing" >&2
  exit 1
fi
unzip -p "${SOURCE_ZIP}" install.md | grep -q 'tools/release/install_local.sh'

"${REPO_ROOT}/tools/release/verify_release_package.sh" "${SOURCE_ZIP}" "${CHECKSUMS}"

grep -q "UECommandForge-${VERSION}-Source.zip" "${CHECKSUMS}"

BAD_SOURCE_DIR="${WORK_DIR}/bad-source"
mkdir -p "${BAD_SOURCE_DIR}"
unzip -q "${SOURCE_ZIP}" -d "${BAD_SOURCE_DIR}/package"
mkdir -p "${BAD_SOURCE_DIR}/package/sample/Saved"
printf 'unexpected\n' > "${BAD_SOURCE_DIR}/package/sample/Saved/leak.txt"
(
  cd "${BAD_SOURCE_DIR}/package"
  uecf_create_zip "${BAD_SOURCE_DIR}/saved-leak.zip" ./*
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

SOURCE_REPARSE_TARGET="${WORK_DIR}/source-reparse-target"
SOURCE_REPARSE_OUT="${WORK_DIR}/source-reparse"
SOURCE_REPARSE_SENTINEL="${SOURCE_REPARSE_OUT}/UECommandForge-${VERSION}-Source/sentinel.txt"
mkdir -p "${SOURCE_REPARSE_TARGET}" "$(dirname "${SOURCE_REPARSE_SENTINEL}")"
printf 'preserve source output\n' > "${SOURCE_REPARSE_SENTINEL}"
if ! create_reparse_fixture "${SOURCE_REPARSE_LINK}" "${SOURCE_REPARSE_TARGET}"; then
  echo "could not create source package input reparse fixture" >&2
  exit 1
fi
if "${REPO_ROOT}/tools/release/package_source.sh" \
  --version "${VERSION}" \
  --channel source-reparse \
  --out-dir "${SOURCE_REPARSE_OUT}" >/dev/null 2>&1; then
  echo "source package should reject reparse points in source tree" >&2
  exit 1
fi
grep -q 'preserve source output' "${SOURCE_REPARSE_SENTINEL}"
remove_reparse_fixture "${SOURCE_REPARSE_LINK}"

OUTPUT_REPARSE_OUT="${WORK_DIR}/output-child-reparse"
OUTPUT_REPARSE_DIR="${OUTPUT_REPARSE_OUT}/UECommandForge-${VERSION}-Source"
OUTPUT_REPARSE_TARGET="${WORK_DIR}/output-child-target"
OUTPUT_REPARSE_LINK="${OUTPUT_REPARSE_DIR}/tools"
mkdir -p "${OUTPUT_REPARSE_DIR}" "${OUTPUT_REPARSE_TARGET}"
printf 'preserve source output child target\n' > "${OUTPUT_REPARSE_TARGET}/sentinel.txt"
if ! create_reparse_fixture "${OUTPUT_REPARSE_LINK}" "${OUTPUT_REPARSE_TARGET}"; then
  echo "could not create source package output child reparse fixture" >&2
  exit 1
fi
if "${REPO_ROOT}/tools/release/package_source.sh" \
  --version "${VERSION}" \
  --channel output-child-reparse \
  --out-dir "${OUTPUT_REPARSE_OUT}" >/dev/null 2>&1; then
  echo "source package should reject existing output reparse child" >&2
  exit 1
fi
grep -q 'preserve source output child target' "${OUTPUT_REPARSE_TARGET}/sentinel.txt"
remove_reparse_fixture "${OUTPUT_REPARSE_LINK}"
OUTPUT_REPARSE_LINK=""
