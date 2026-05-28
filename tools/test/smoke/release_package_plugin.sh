#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
WORK_DIR="${REPO_ROOT}/sample/Saved/CodexReports/ReleasePackagePlugin"
VERSION="0.8.0-test"

rm -rf "${WORK_DIR}"
mkdir -p "${WORK_DIR}"

"${REPO_ROOT}/tools/release/package_plugin.sh" \
  --version "${VERSION}" \
  --channel local-smoke \
  --out-dir "${WORK_DIR}/relative-check/.." \
  --skip-build

PLUGIN_ZIP="${WORK_DIR}/UECommandForge-${VERSION}-UE5.7-Mac.zip"
CHECKSUMS="${WORK_DIR}/checksums.txt"
ZIP_LIST="${WORK_DIR}/zip-files.txt"

test -f "${PLUGIN_ZIP}"
test -f "${CHECKSUMS}"

unzip -Z1 "${PLUGIN_ZIP}" > "${ZIP_LIST}"
grep -q '^UECommandForge.uplugin$' "${ZIP_LIST}"
grep -q '^Binaries/Mac/UnrealEditor-UECommandForgeRuntime.dylib$' "${ZIP_LIST}"
grep -q '^Binaries/Mac/UnrealEditor-UECommandForgeEditor.dylib$' "${ZIP_LIST}"
grep -q '^Source/UECommandForgeRuntime/UECommandForgeRuntime.Build.cs$' "${ZIP_LIST}"
grep -q '^Source/UECommandForgeEditor/UECommandForgeEditor.Build.cs$' "${ZIP_LIST}"
grep -q '^uecommandforge-manifest.json$' "${ZIP_LIST}"
grep -q '^install.md$' "${ZIP_LIST}"
grep -q '^release-notes.md$' "${ZIP_LIST}"

unzip -p "${PLUGIN_ZIP}" uecommandforge-manifest.json | jq -e \
  --arg version "${VERSION}" \
  '.version == $version
   and .engine_version == "UE5.7"
   and .release_channel == "local-smoke"
   and (.plugin_files | index("UECommandForge.uplugin"))
   and (.plugin_files | index("Binaries/Mac/UnrealEditor-UECommandForgeRuntime.dylib"))
   and (.plugin_files | index("Binaries/Mac/UnrealEditor-UECommandForgeEditor.dylib"))
   and (.install_commands[] | select(. == "install-uecommandforge.sh --project <path-to-uproject>"))
   and (.post_install_checks[] | select(. == "Hello commandlet"))' >/dev/null

"${REPO_ROOT}/tools/release/verify_release_package.sh" "${PLUGIN_ZIP}"

grep -q "UECommandForge-${VERSION}-UE5.7-Mac.zip" "${CHECKSUMS}"

if "${REPO_ROOT}/tools/release/package_plugin.sh" \
  --version '../bad' \
  --out-dir "${WORK_DIR}/bad" \
  --skip-build >/dev/null 2>&1; then
  echo "unsafe version should fail" >&2
  exit 1
fi
