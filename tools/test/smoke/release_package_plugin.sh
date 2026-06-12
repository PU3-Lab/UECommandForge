#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
# shellcheck disable=SC1091
source "${REPO_ROOT}/tools/release/common.sh"
WORK_DIR="${REPO_ROOT}/sample/Saved/UECommandForge/Reports/ReleasePackagePlugin"
VERSION="$(jq -r '.VersionName' "${REPO_ROOT}/sample/Plugins/UECommandForge/UECommandForge.uplugin")"
PLATFORM="Mac"
RUNTIME_BINARY="Binaries/Mac/UnrealEditor-UECommandForgeRuntime.dylib"
EDITOR_BINARY="Binaries/Mac/UnrealEditor-UECommandForgeEditor.dylib"
BUILD_REPARSE_LINK="${REPO_ROOT}/sample/Saved/PluginBuild/.release_package_reparse_fixture"
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
  remove_reparse_fixture "${BUILD_REPARSE_LINK}"
  remove_reparse_fixture "${OUTPUT_REPARSE_LINK}"
}
trap cleanup EXIT

case "$(uname -s)" in
  Darwin)
    PLATFORM="Mac"
    RUNTIME_BINARY="Binaries/Mac/UnrealEditor-UECommandForgeRuntime.dylib"
    EDITOR_BINARY="Binaries/Mac/UnrealEditor-UECommandForgeEditor.dylib"
    ;;
  Linux)
    PLATFORM="Linux"
    RUNTIME_BINARY="Binaries/Linux/UnrealEditor-UECommandForgeRuntime.so"
    EDITOR_BINARY="Binaries/Linux/UnrealEditor-UECommandForgeEditor.so"
    ;;
  MINGW*|MSYS*|CYGWIN*)
    PLATFORM="Win64"
    RUNTIME_BINARY="Binaries/Win64/UnrealEditor-UECommandForgeRuntime.dll"
    EDITOR_BINARY="Binaries/Win64/UnrealEditor-UECommandForgeEditor.dll"
    ;;
esac

rm -rf "${WORK_DIR}"
mkdir -p "${WORK_DIR}"

PACKAGE_ARGS=(
  --version "${VERSION}" \
  --channel local-smoke \
  --out-dir "${WORK_DIR}/relative-check/.."
)

if [ "${UECF_RELEASE_PLUGIN_SKIP_BUILD:-0}" = "1" ]; then
  PACKAGE_ARGS+=(--skip-build)
fi

"${REPO_ROOT}/tools/release/package_plugin.sh" "${PACKAGE_ARGS[@]}"

PLUGIN_ZIP="${WORK_DIR}/UECommandForge-${VERSION}-UE5.7-${PLATFORM}.zip"
CHECKSUMS="${WORK_DIR}/checksums.txt"
ZIP_LIST="${WORK_DIR}/zip-files.txt"

test -f "${PLUGIN_ZIP}"
test -f "${CHECKSUMS}"

unzip -Z1 "${PLUGIN_ZIP}" > "${ZIP_LIST}"
grep -q '^UECommandForge.uplugin$' "${ZIP_LIST}"
grep -q "^${RUNTIME_BINARY}$" "${ZIP_LIST}"
grep -q "^${EDITOR_BINARY}$" "${ZIP_LIST}"
grep -q '^Source/UECommandForgeRuntime/UECommandForgeRuntime.Build.cs$' "${ZIP_LIST}"
grep -q '^Source/UECommandForgeEditor/UECommandForgeEditor.Build.cs$' "${ZIP_LIST}"
grep -q '^uecommandforge-manifest.json$' "${ZIP_LIST}"
grep -q '^install.md$' "${ZIP_LIST}"
grep -q '^release-notes.md$' "${ZIP_LIST}"
grep -q '^validation-report.json$' "${ZIP_LIST}"

unzip -p "${PLUGIN_ZIP}" uecommandforge-manifest.json | jq -e \
  --arg version "${VERSION}" \
  --arg runtime_binary "${RUNTIME_BINARY}" \
  --arg editor_binary "${EDITOR_BINARY}" \
  '.version == $version
   and .engine_version == "UE5.7"
   and .release_channel == "local-smoke"
   and (.plugin_files | index("UECommandForge.uplugin"))
   and (.plugin_files | index($runtime_binary))
   and (.plugin_files | index($editor_binary))
   and (.install_commands | length == 0)
   and (.checksums[$runtime_binary] | type == "string")
   and (.checksums["install.md"] | type == "string")
   and (.checksums["release-notes.md"] | type == "string")
   and (.checksums["validation-report.json"] | type == "string")
   and (.post_install_checks[] | select(. == "Hello commandlet"))' >/dev/null
unzip -p "${PLUGIN_ZIP}" UECommandForge.uplugin | \
  jq -e --arg version "${VERSION}" '.VersionName == $version' >/dev/null

unzip -p "${PLUGIN_ZIP}" validation-report.json | jq -e \
  --arg version "${VERSION}" \
  --arg platform "${PLATFORM}" \
  '.version == $version
   and .package_type == "plugin"
   and .platform == $platform
   and .status == "pass"
   and (.checks[] | select(.name == "generated plugin package"))' >/dev/null

"${REPO_ROOT}/tools/release/verify_release_package.sh" "${PLUGIN_ZIP}" "${CHECKSUMS}"

grep -q "UECommandForge-${VERSION}-UE5.7-${PLATFORM}.zip" "${CHECKSUMS}"

if "${REPO_ROOT}/tools/release/package_plugin.sh" \
  --version '../bad' \
  --out-dir "${WORK_DIR}/bad" \
  --skip-build >/dev/null 2>&1; then
  echo "unsafe version should fail" >&2
  exit 1
fi

if "${REPO_ROOT}/tools/release/package_plugin.sh" \
  --version '0.0.0-mismatch' \
  --out-dir "${WORK_DIR}/bad-version" \
  --skip-build >/dev/null 2>&1; then
  echo "mismatched plugin version should fail" >&2
  exit 1
fi

if [ "${PLATFORM}" != "Win64" ]; then
  if "${REPO_ROOT}/tools/release/package_plugin.sh" \
    --version "${VERSION}" \
    --platform Win64 \
    --out-dir "${WORK_DIR}/bad-platform" \
    --skip-build >/dev/null 2>&1; then
    echo "missing platform build output should fail" >&2
    exit 1
  fi
fi

BUILD_REPARSE_TARGET="${WORK_DIR}/build-reparse-target"
BUILD_REPARSE_OUT="${WORK_DIR}/build-reparse"
BUILD_REPARSE_SENTINEL="${BUILD_REPARSE_OUT}/UECommandForge-${VERSION}-UE5.7-${PLATFORM}/sentinel.txt"
mkdir -p "${BUILD_REPARSE_TARGET}" "$(dirname "${BUILD_REPARSE_SENTINEL}")"
printf 'preserve plugin output\n' > "${BUILD_REPARSE_SENTINEL}"
if ! create_reparse_fixture "${BUILD_REPARSE_LINK}" "${BUILD_REPARSE_TARGET}"; then
  echo "could not create plugin build reparse fixture" >&2
  exit 1
fi
if "${REPO_ROOT}/tools/release/package_plugin.sh" \
  --version "${VERSION}" \
  --channel build-reparse \
  --out-dir "${BUILD_REPARSE_OUT}" \
  --skip-build >/dev/null 2>&1; then
  echo "plugin package should reject reparse points in build tree" >&2
  exit 1
fi
grep -q 'preserve plugin output' "${BUILD_REPARSE_SENTINEL}"
remove_reparse_fixture "${BUILD_REPARSE_LINK}"

OUTPUT_REPARSE_OUT="${WORK_DIR}/output-child-reparse"
OUTPUT_REPARSE_DIR="${OUTPUT_REPARSE_OUT}/UECommandForge-${VERSION}-UE5.7-${PLATFORM}"
OUTPUT_REPARSE_TARGET="${WORK_DIR}/output-child-target"
OUTPUT_REPARSE_LINK="${OUTPUT_REPARSE_DIR}/Binaries"
mkdir -p "${OUTPUT_REPARSE_DIR}" "${OUTPUT_REPARSE_TARGET}"
printf 'preserve plugin output child target\n' > "${OUTPUT_REPARSE_TARGET}/sentinel.txt"
if ! create_reparse_fixture "${OUTPUT_REPARSE_LINK}" "${OUTPUT_REPARSE_TARGET}"; then
  echo "could not create plugin output child reparse fixture" >&2
  exit 1
fi
if "${REPO_ROOT}/tools/release/package_plugin.sh" \
  --version "${VERSION}" \
  --channel output-child-reparse \
  --out-dir "${OUTPUT_REPARSE_OUT}" \
  --skip-build >/dev/null 2>&1; then
  echo "plugin package should reject existing output reparse child" >&2
  exit 1
fi
grep -q 'preserve plugin output child target' "${OUTPUT_REPARSE_TARGET}/sentinel.txt"
remove_reparse_fixture "${OUTPUT_REPARSE_LINK}"
OUTPUT_REPARSE_LINK=""
