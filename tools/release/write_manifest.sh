#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/common.sh"

PACKAGE_ROOT=""
VERSION=""
CHANNEL="local"
OUTPUT=""
ENGINE_VERSION="UE5.7"
PACKAGE_TYPE="generic"
INSTALL_COMMANDS='[]'

while [ $# -gt 0 ]; do
  case "$1" in
    --package-root)
      PACKAGE_ROOT="$2"
      shift 2
      ;;
    --version)
      VERSION="$2"
      shift 2
      ;;
    --channel)
      CHANNEL="$2"
      shift 2
      ;;
    --output)
      OUTPUT="$2"
      shift 2
      ;;
    --package-type)
      PACKAGE_TYPE="$2"
      shift 2
      ;;
    --install-command)
      INSTALL_COMMANDS="$(
        jq --arg command "$2" '. + [$command]' <<< "${INSTALL_COMMANDS}"
      )"
      shift 2
      ;;
    *)
      echo "[write_manifest] Unknown argument: $1" >&2
      exit 2
      ;;
  esac
done

if [ -z "${PACKAGE_ROOT}" ] || [ -z "${VERSION}" ] || [ -z "${OUTPUT}" ]; then
  echo "[write_manifest] --package-root, --version, and --output are required" >&2
  exit 2
fi

if [ ! -d "${PACKAGE_ROOT}" ]; then
  echo "[write_manifest] package root not found: ${PACKAGE_ROOT}" >&2
  exit 2
fi

uecf_reject_link_ancestors "${PACKAGE_ROOT}" "write_manifest"
uecf_reject_link_tree "${PACKAGE_ROOT}" "write_manifest"
uecf_reject_output_file_path "${OUTPUT}" "write_manifest"

TMP_DIR="$(mktemp -d)"
cleanup() {
  rm -rf "${TMP_DIR}"
}
trap cleanup EXIT

files_json() {
  local root="$1"
  local dir="$2"
  if [ -d "${root}/${dir}" ]; then
    (
      cd "${root}"
      find "${dir}" -type f | sort | jq -R -s 'split("\n")[:-1]'
    )
  else
    printf '[]'
  fi
}

plugin_files_path="${TMP_DIR}/plugin_files.json"
tool_files_path="${TMP_DIR}/tool_files.json"
spec_files_path="${TMP_DIR}/spec_files.json"
checksums_path="${TMP_DIR}/checksums.json"
install_commands_path="${TMP_DIR}/install_commands.json"

(
  cd "${PACKAGE_ROOT}"
  find . -type f \
    ! -path './tools/*' \
    ! -path './specs/*' \
    ! -path './uecommandforge-manifest.json' \
    ! -path './install.md' \
    ! -path './release-notes.md' \
    ! -path './validation-report.json' \
    | sed 's#^\./##' \
    | sort \
    | jq -R -s 'split("\n")[:-1]'
) > "${plugin_files_path}"
files_json "${PACKAGE_ROOT}" tools > "${tool_files_path}"
files_json "${PACKAGE_ROOT}" specs > "${spec_files_path}"
printf '%s\n' "${INSTALL_COMMANDS}" > "${install_commands_path}"
(
  cd "${PACKAGE_ROOT}"
  find . -type f \
    ! -path './uecommandforge-manifest.json' \
    | sed 's#^\./##' \
    | sort \
    | while IFS= read -r path; do
      hash="$(uecf_sha256 "${path}")" || exit 2
      printf '%s\t%s\n' "${path}" "${hash}"
    done \
    | jq -R -s '
        split("\n")[:-1]
        | map(split("\t"))
        | map({key: .[0], value: .[1]})
        | from_entries
      '
) > "${checksums_path}"

manifest_temp="$(uecf_prepare_output_temp_file "${OUTPUT}" "write_manifest")"
if ! jq -n \
  --arg version "${VERSION}" \
  --arg engine_version "${ENGINE_VERSION}" \
  --arg release_channel "${CHANNEL}" \
  --arg package_type "${PACKAGE_TYPE}" \
  --slurpfile plugin_files "${plugin_files_path}" \
  --slurpfile tool_files "${tool_files_path}" \
  --slurpfile spec_files "${spec_files_path}" \
  --slurpfile checksums "${checksums_path}" \
  --slurpfile install_commands "${install_commands_path}" \
  '{
    version: $version,
    engine_version: $engine_version,
    release_channel: $release_channel,
    package_type: $package_type,
    plugin_files: $plugin_files[0],
    tool_files: $tool_files[0],
    spec_files: $spec_files[0],
    checksums: $checksums[0],
    install_commands: $install_commands[0],
    post_install_checks: (
      if $package_type == "source" then
        ["source package file list", "source package checksum"]
      elif $package_type == "tools" then
        ["Hello commandlet", "Codex tools path", "project link manifest", "Blueprint defaults wrapper"]
      else
        ["Hello commandlet", "Codex tools path", "project link manifest"]
      end
    ),
    known_limits: [
      "Windows wrapper execution requires verification on a Windows host."
    ]
  }' > "${manifest_temp}"; then
  rm -f "${manifest_temp}"
  exit 2
fi
uecf_commit_output_temp_file "${manifest_temp}" "${OUTPUT}" "write_manifest"
