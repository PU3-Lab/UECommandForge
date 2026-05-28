#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
PLUGIN_DESCRIPTOR="${REPO_ROOT}/sample/Plugins/UECommandForge/UECommandForge.uplugin"
BUILD_DIR="${REPO_ROOT}/sample/Saved/PluginBuild"
VERSION="$(jq -r '.VersionName' "${PLUGIN_DESCRIPTOR}")"
CHANNEL="local"
OUT_DIR="${REPO_ROOT}/sample/Saved/Release"
SKIP_BUILD=false

case "$(uname -s)" in
  Darwin) PLATFORM="Mac" ;;
  Linux) PLATFORM="Linux" ;;
  MINGW*|MSYS*|CYGWIN*) PLATFORM="Win64" ;;
  *) PLATFORM="Unknown" ;;
esac

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
    --platform)
      PLATFORM="$2"
      shift 2
      ;;
    --skip-build)
      SKIP_BUILD=true
      shift
      ;;
    *)
      echo "[package_plugin] Unknown argument: $1" >&2
      exit 2
      ;;
  esac
done

if [ -z "${VERSION}" ]; then
  echo "[package_plugin] version is required" >&2
  exit 2
fi

case "${VERSION}" in
  *[!A-Za-z0-9._-]*)
    echo "[package_plugin] version contains unsafe characters: ${VERSION}" >&2
    exit 2
    ;;
esac

case "${CHANNEL}" in
  *[!A-Za-z0-9._-]*)
    echo "[package_plugin] channel contains unsafe characters: ${CHANNEL}" >&2
    exit 2
    ;;
esac

case "${PLATFORM}" in
  Mac|Linux|Win64) ;;
  *)
    echo "[package_plugin] unsupported platform: ${PLATFORM}" >&2
    exit 2
    ;;
esac

if [ "${SKIP_BUILD}" != true ]; then
  "${REPO_ROOT}/tools/ue/build_plugin.sh"
fi

if [ ! -f "${BUILD_DIR}/UECommandForge.uplugin" ]; then
  echo "[package_plugin] plugin build output not found: ${BUILD_DIR}" >&2
  exit 2
fi

descriptor_version="$(jq -r '.VersionName' "${BUILD_DIR}/UECommandForge.uplugin")"
if [ -z "${descriptor_version}" ] || [ "${descriptor_version}" = "null" ]; then
  echo "[package_plugin] plugin VersionName is missing" >&2
  exit 2
fi

mkdir -p "${OUT_DIR}"
OUT_DIR="$(cd "${OUT_DIR}" && pwd)"
PACKAGE_NAME="UECommandForge-${VERSION}-UE5.7-${PLATFORM}"
PACKAGE_DIR="${OUT_DIR}/${PACKAGE_NAME}"
ZIP_PATH="${OUT_DIR}/${PACKAGE_NAME}.zip"
CHECKSUMS_PATH="${OUT_DIR}/checksums.txt"

case "${PACKAGE_DIR}" in
  "${OUT_DIR}/UECommandForge-"*"-UE5.7-"*) ;;
  *)
    echo "[package_plugin] unsafe package dir: ${PACKAGE_DIR}" >&2
    exit 2
    ;;
esac

rm -rf "${PACKAGE_DIR}" "${ZIP_PATH}"
mkdir -p "${PACKAGE_DIR}"

(
  cd "${BUILD_DIR}"
  tar --exclude='./Intermediate' \
    --exclude='./Saved' \
    --exclude='./DerivedDataCache' \
    -cf - .
) | (
  cd "${PACKAGE_DIR}"
  tar -xf -
)

"${SCRIPT_DIR}/write_manifest.sh" \
  --package-root "${PACKAGE_DIR}" \
  --version "${VERSION}" \
  --channel "${CHANNEL}" \
  --output "${PACKAGE_DIR}/uecommandforge-manifest.json"

cat > "${PACKAGE_DIR}/install.md" <<INSTALL
# UECommandForge Plugin Install

Install this package into the target Unreal project:

\`\`\`
<Project>/Plugins/UECommandForge
\`\`\`

Codex-side tools and specs are installed separately into:

\`\`\`
~/.codex/UECommandForge
\`\`\`
INSTALL

cat > "${PACKAGE_DIR}/release-notes.md" <<NOTES
# UECommandForge ${VERSION} Plugin

- Release channel: ${CHANNEL}
- Engine: UE5.7
- Platform: ${PLATFORM}
- Source plugin VersionName: ${descriptor_version}
NOTES

(
  cd "${PACKAGE_DIR}"
  zip -qr "${ZIP_PATH}" ./*
)

"${SCRIPT_DIR}/write_checksums.sh" "${ZIP_PATH}" > "${CHECKSUMS_PATH}"
"${SCRIPT_DIR}/verify_release_package.sh" "${ZIP_PATH}" >/dev/null

echo "${ZIP_PATH}"
