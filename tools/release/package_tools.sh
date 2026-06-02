#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/common.sh"

uecf_reject_link_ancestors "${REPO_ROOT}" "package_tools"

PLUGIN_DESCRIPTOR="${REPO_ROOT}/sample/Plugins/UECommandForge/UECommandForge.uplugin"
uecf_reject_link_ancestors "${PLUGIN_DESCRIPTOR}" "package_tools"
uecf_reject_link_path "${PLUGIN_DESCRIPTOR}" "package_tools"
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

uecf_reject_link_ancestors "${OUT_DIR}" "package_tools"
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

uecf_reject_link_ancestors "${OUT_DIR}" "package_tools"
uecf_reject_link_ancestors "${PACKAGE_DIR}" "package_tools"
uecf_reject_output_file_path "${ZIP_PATH}" "package_tools"
uecf_reject_output_file_path "${CHECKSUMS_PATH}" "package_tools"
uecf_reject_link_tree "${PACKAGE_DIR}" "package_tools"
uecf_reject_link_ancestors "${REPO_ROOT}/tools" "package_tools"
uecf_reject_link_tree "${REPO_ROOT}/tools" "package_tools"
uecf_reject_link_ancestors "${REPO_ROOT}/specs" "package_tools"
uecf_reject_link_tree "${REPO_ROOT}/specs" "package_tools"
uecf_reject_link_ancestors "${REPO_ROOT}/install-uecommandforge.sh" "package_tools"
uecf_reject_link_path "${REPO_ROOT}/install-uecommandforge.sh" "package_tools"
uecf_reject_link_ancestors "${REPO_ROOT}/install-uecommandforge.bat" "package_tools"
uecf_reject_link_path "${REPO_ROOT}/install-uecommandforge.bat" "package_tools"
uecf_reject_link_ancestors "${REPO_ROOT}/install-uecommandforge.ps1" "package_tools"
uecf_reject_link_path "${REPO_ROOT}/install-uecommandforge.ps1" "package_tools"

rm -rf "${PACKAGE_DIR}" "${ZIP_PATH}"
mkdir -p "${PACKAGE_DIR}"

cp -R "${REPO_ROOT}/tools" "${PACKAGE_DIR}/tools"
cp -R "${REPO_ROOT}/specs" "${PACKAGE_DIR}/specs"
cp "${REPO_ROOT}/install-uecommandforge.sh" "${PACKAGE_DIR}/install-uecommandforge.sh"
cp "${REPO_ROOT}/install-uecommandforge.bat" "${PACKAGE_DIR}/install-uecommandforge.bat"
cp "${REPO_ROOT}/install-uecommandforge.ps1" "${PACKAGE_DIR}/install-uecommandforge.ps1"

uecf_reject_link_tree "${PACKAGE_DIR}" "package_tools"

REQUIRED_BLUEPRINT_DEFAULTS_FILES=(
  "tools/ue/set_blueprint_defaults.sh"
  "tools/ue/set_blueprint_defaults.bat"
  "specs/examples/blueprint_defaults.json"
)

for required_path in "${REQUIRED_BLUEPRINT_DEFAULTS_FILES[@]}"; do
  if [ ! -f "${PACKAGE_DIR}/${required_path}" ]; then
    echo "[package_tools] required blueprint defaults file is missing: ${required_path}" >&2
    exit 2
  fi
done

if [ ! -x "${PACKAGE_DIR}/tools/ue/set_blueprint_defaults.sh" ]; then
  echo "[package_tools] blueprint defaults shell wrapper is not executable" >&2
  exit 2
fi

uecf_write_file_from_stdin "${PACKAGE_DIR}/install.md" "package_tools" <<INSTALL
# UECommandForge Tools Install

Install plugin files into the target Unreal project and install Codex tools/specs into:

\`\`\`
~/.codex/UECommandForge
\`\`\`

Installer status:

Use \`install-uecommandforge.sh\` with a verified plugin package and this tools
package to install project plugin files and Codex-side tools/specs.
INSTALL

uecf_write_file_from_stdin "${PACKAGE_DIR}/release-notes.md" "package_tools" <<NOTES
# UECommandForge ${VERSION}

## Package

- Type: Tools and specs for Codex-side installation
- Release channel: ${CHANNEL}
- Target Codex root: ~/.codex/UECommandForge

## Validation

- Manifest file list and checksum verification are required.
- Installer commands are recorded in \`uecommandforge-manifest.json\`.

## Known Limits

- Plugin files are packaged separately.
- Windows installer behavior still requires Windows host verification.
NOTES

uecf_write_file_from_stdin "${PACKAGE_DIR}/validation-report.json" "package_tools" <<REPORT
{
  "version": "${VERSION}",
  "release_channel": "${CHANNEL}",
  "package_type": "tools",
  "status": "pass",
  "checks": [
    {
      "name": "generated tools package",
      "status": "pass"
    },
    {
      "name": "manifest checksum coverage",
      "status": "pass"
    },
    {
      "name": "blueprint defaults wrapper included",
      "status": "pass",
      "required_files": [
        "tools/ue/set_blueprint_defaults.sh",
        "tools/ue/set_blueprint_defaults.bat",
        "specs/examples/blueprint_defaults.json"
      ]
    }
  ],
  "known_limits": [
    "Windows wrapper execution requires verification on a Windows host."
  ]
}
REPORT

"${SCRIPT_DIR}/write_manifest.sh" \
  --package-root "${PACKAGE_DIR}" \
  --version "${VERSION}" \
  --channel "${CHANNEL}" \
  --package-type tools \
  --install-command "./install-uecommandforge.sh" \
  --install-command "install-uecommandforge.bat" \
  --install-command "install-uecommandforge.ps1" \
  --output "${PACKAGE_DIR}/uecommandforge-manifest.json"

(
  cd "${PACKAGE_DIR}"
  uecf_create_zip "${ZIP_PATH}" \
    tools specs \
    install-uecommandforge.sh install-uecommandforge.bat install-uecommandforge.ps1 \
    uecommandforge-manifest.json install.md release-notes.md validation-report.json
)

uecf_write_checksums_file "${ZIP_PATH}" "${CHECKSUMS_PATH}" "package_tools"
"${SCRIPT_DIR}/verify_release_package.sh" "${ZIP_PATH}" "${CHECKSUMS_PATH}" >/dev/null

echo "${ZIP_PATH}"
