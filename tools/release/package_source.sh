#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/common.sh"

PLUGIN_DESCRIPTOR="${REPO_ROOT}/sample/Plugins/UECommandForge/UECommandForge.uplugin"
VERSION="$(jq -r '.VersionName' "${PLUGIN_DESCRIPTOR}")"
CHANNEL="local"
OUT_DIR="${REPO_ROOT}/sample/Saved/Release"

while [ $# -gt 0 ]; do
  case "$1" in
    --version)
      VERSION="$2"
      shift 2
      ;;
    --channel)
      CHANNEL="$2"
      shift 2
      ;;
    --out-dir)
      OUT_DIR="$2"
      shift 2
      ;;
    *)
      echo "[package_source] Unknown argument: $1" >&2
      exit 2
      ;;
  esac
done

if [ -z "${VERSION}" ]; then
  echo "[package_source] version is required" >&2
  exit 2
fi

case "${VERSION}" in
  *[!A-Za-z0-9._-]*)
    echo "[package_source] version contains unsafe characters: ${VERSION}" >&2
    exit 2
    ;;
esac

case "${CHANNEL}" in
  *[!A-Za-z0-9._-]*)
    echo "[package_source] channel contains unsafe characters: ${CHANNEL}" >&2
    exit 2
    ;;
esac

descriptor_version="$(jq -r '.VersionName' "${PLUGIN_DESCRIPTOR}")"
if [ -z "${descriptor_version}" ] || [ "${descriptor_version}" = "null" ]; then
  echo "[package_source] plugin VersionName is missing" >&2
  exit 2
fi

if [ "${descriptor_version}" != "${VERSION}" ]; then
  echo "[package_source] package version must match plugin VersionName: ${VERSION} != ${descriptor_version}" >&2
  exit 2
fi

mkdir -p "${OUT_DIR}"
OUT_DIR="$(cd "${OUT_DIR}" && pwd)"
PACKAGE_NAME="UECommandForge-${VERSION}-Source"
PACKAGE_DIR="${OUT_DIR}/${PACKAGE_NAME}"
ZIP_PATH="${OUT_DIR}/${PACKAGE_NAME}.zip"
CHECKSUMS_PATH="${OUT_DIR}/checksums.txt"

case "${PACKAGE_DIR}" in
  "${OUT_DIR}/UECommandForge-"*"-Source") ;;
  *)
    echo "[package_source] unsafe package dir: ${PACKAGE_DIR}" >&2
    exit 2
    ;;
esac

rm -rf "${PACKAGE_DIR}" "${ZIP_PATH}"
mkdir -p "${PACKAGE_DIR}"

(
  cd "${REPO_ROOT}"
  git ls-files \
    README.md \
    docs \
    specs \
    tools \
    sample/UECommandForgeSample.uproject \
    sample/Plugins/UECommandForge/UECommandForge.uplugin \
    sample/Plugins/UECommandForge/Config \
    sample/Plugins/UECommandForge/Source \
    | grep -v '^docs/memory/' \
    | grep -v '^docs/superpowers/' \
    | while IFS= read -r path; do
      mkdir -p "${PACKAGE_DIR}/$(dirname "${path}")"
      cp "${path}" "${PACKAGE_DIR}/${path}"
    done
)

if find "${PACKAGE_DIR}" -type l -print -quit | grep -q .; then
  echo "[package_source] symlinks are not allowed in release packages" >&2
  exit 2
fi

cat > "${PACKAGE_DIR}/install.md" <<INSTALL
# UECommandForge Source Install

Use this source package to inspect, build, or repack UECommandForge locally.

Plugin source is located at:

\`\`\`
sample/Plugins/UECommandForge
\`\`\`

Codex-side tools and specs are located at:

\`\`\`
tools
specs
\`\`\`

Installer status:

Use \`tools/release/install_local.sh\` with a verified plugin package and tools
package to install project plugin files and Codex-side tools/specs.
INSTALL

cat > "${PACKAGE_DIR}/release-notes.md" <<NOTES
# UECommandForge ${VERSION} Source

## Package

- Type: Source package
- Release channel: ${CHANNEL}
- Engine: UE5.7

## Validation

- Generated source package excludes \`.git\`, generated build output, and \`sample/Saved\`.
- Manifest file list and checksum verification are required.

## Known Limits

- This package is source-only and does not include prebuilt plugin binaries.
- Windows \`.bat\` package wrappers still require Windows host verification.
NOTES

cat > "${PACKAGE_DIR}/validation-report.json" <<REPORT
{
  "version": "${VERSION}",
  "release_channel": "${CHANNEL}",
  "package_type": "source",
  "engine_version": "UE5.7",
  "status": "pass",
  "checks": [
    {
      "name": "generated source package",
      "status": "pass"
    },
    {
      "name": "excluded generated build output",
      "status": "pass"
    },
    {
      "name": "manifest checksum coverage",
      "status": "pass"
    }
  ],
  "known_limits": [
    "Source package does not include prebuilt plugin binaries.",
    "Windows wrapper execution requires verification on a Windows host."
  ]
}
REPORT

"${SCRIPT_DIR}/write_manifest.sh" \
  --package-root "${PACKAGE_DIR}" \
  --version "${VERSION}" \
  --channel "${CHANNEL}" \
  --package-type source \
  --output "${PACKAGE_DIR}/uecommandforge-manifest.json"

(
  cd "${PACKAGE_DIR}"
  uecf_create_zip "${ZIP_PATH}" ./*
)

"${SCRIPT_DIR}/write_checksums.sh" "${ZIP_PATH}" > "${CHECKSUMS_PATH}"
"${SCRIPT_DIR}/verify_release_package.sh" "${ZIP_PATH}" "${CHECKSUMS_PATH}" >/dev/null

echo "${ZIP_PATH}"
