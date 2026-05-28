#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
WORK_DIR="${REPO_ROOT}/sample/Saved/CodexReports/ReleasePackagePlugin"
VERSION="$(jq -r '.VersionName' "${REPO_ROOT}/sample/Plugins/UECommandForge/UECommandForge.uplugin")"
PLATFORM="Mac"
RUNTIME_BINARY="Binaries/Mac/UnrealEditor-UECommandForgeRuntime.dylib"
EDITOR_BINARY="Binaries/Mac/UnrealEditor-UECommandForgeEditor.dylib"

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
   and (.post_install_checks[] | select(. == "Hello commandlet"))' >/dev/null
jq -e --arg version "${VERSION}" '.VersionName == $version' \
  <(unzip -p "${PLUGIN_ZIP}" UECommandForge.uplugin) >/dev/null

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
