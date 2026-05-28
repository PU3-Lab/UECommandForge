#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

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
      echo "[package_tools] Unknown argument: $1" >&2
      exit 2
      ;;
  esac
done

if [ -z "${VERSION}" ]; then
  echo "[package_tools] version is required" >&2
  exit 2
fi

case "${VERSION}" in
  *[!A-Za-z0-9._-]*)
    echo "[package_tools] version contains unsafe characters: ${VERSION}" >&2
    exit 2
    ;;
esac

case "${CHANNEL}" in
  *[!A-Za-z0-9._-]*)
    echo "[package_tools] channel contains unsafe characters: ${CHANNEL}" >&2
    exit 2
    ;;
esac

mkdir -p "${OUT_DIR}"
OUT_DIR="$(cd "${OUT_DIR}" && pwd)"
PACKAGE_NAME="UECommandForge-${VERSION}-Tools"
PACKAGE_DIR="${OUT_DIR}/${PACKAGE_NAME}"
ZIP_PATH="${OUT_DIR}/${PACKAGE_NAME}.zip"
CHECKSUMS_PATH="${OUT_DIR}/checksums.txt"

case "${PACKAGE_DIR}" in
  "${OUT_DIR}/UECommandForge-"*"-Tools") ;;
  *)
    echo "[package_tools] unsafe package dir: ${PACKAGE_DIR}" >&2
    exit 2
    ;;
esac

rm -rf "${PACKAGE_DIR}" "${ZIP_PATH}"
mkdir -p "${PACKAGE_DIR}"

cp -R "${REPO_ROOT}/tools" "${PACKAGE_DIR}/tools"
cp -R "${REPO_ROOT}/specs" "${PACKAGE_DIR}/specs"

if find "${PACKAGE_DIR}" -type l -print -quit | grep -q .; then
  echo "[package_tools] symlinks are not allowed in release packages" >&2
  exit 2
fi

cat > "${PACKAGE_DIR}/install.md" <<INSTALL
# UECommandForge Tools Install

Install plugin files into the target Unreal project and install Codex tools/specs into:

\`\`\`
~/.codex/UECommandForge
\`\`\`

Installer status:

Task 6 installer scripts are not shipped yet. Until then, copy this package into
\`~/.codex/UECommandForge\` and set \`UECF_PROJECT_FILE\` when invoking wrappers.
INSTALL

cat > "${PACKAGE_DIR}/release-notes.md" <<NOTES
# UECommandForge ${VERSION}

- Release channel: ${CHANNEL}
- Package: Tools and specs for Codex-side installation
- Target Codex root: ~/.codex/UECommandForge
NOTES

"${SCRIPT_DIR}/write_manifest.sh" \
  --package-root "${PACKAGE_DIR}" \
  --version "${VERSION}" \
  --channel "${CHANNEL}" \
  --output "${PACKAGE_DIR}/uecommandforge-manifest.json"

(
  cd "${PACKAGE_DIR}"
  zip -qr "${ZIP_PATH}" tools specs uecommandforge-manifest.json install.md release-notes.md
)

"${SCRIPT_DIR}/write_checksums.sh" "${ZIP_PATH}" > "${CHECKSUMS_PATH}"
"${SCRIPT_DIR}/verify_release_package.sh" "${ZIP_PATH}" "${CHECKSUMS_PATH}" >/dev/null

echo "${ZIP_PATH}"
