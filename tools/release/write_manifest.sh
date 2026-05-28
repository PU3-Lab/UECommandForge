#!/usr/bin/env bash
set -euo pipefail

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

plugin_files="$(
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
)"
tool_files="$(files_json "${PACKAGE_ROOT}" tools)"
spec_files="$(files_json "${PACKAGE_ROOT}" specs)"
checksums="$(
  cd "${PACKAGE_ROOT}"
  find . -type f \
    ! -path './uecommandforge-manifest.json' \
    | sed 's#^\./##' \
    | sort \
    | while IFS= read -r path; do
      printf '%s\t%s\n' "${path}" "$(shasum -a 256 "${path}" | awk '{ print $1 }')"
    done \
    | jq -R -s '
        split("\n")[:-1]
        | map(split("\t"))
        | map({key: .[0], value: .[1]})
        | from_entries
      '
)"

jq -n \
  --arg version "${VERSION}" \
  --arg engine_version "${ENGINE_VERSION}" \
  --arg release_channel "${CHANNEL}" \
  --arg package_type "${PACKAGE_TYPE}" \
  --argjson plugin_files "${plugin_files}" \
  --argjson tool_files "${tool_files}" \
  --argjson spec_files "${spec_files}" \
  --argjson checksums "${checksums}" \
  --argjson install_commands "${INSTALL_COMMANDS}" \
  '{
    version: $version,
    engine_version: $engine_version,
    release_channel: $release_channel,
    package_type: $package_type,
    plugin_files: $plugin_files,
    tool_files: $tool_files,
    spec_files: $spec_files,
    checksums: $checksums,
    install_commands: $install_commands,
    post_install_checks: (
      if $package_type == "source" then
        ["source package file list", "source package checksum"]
      else
        ["Hello commandlet", "Codex tools path", "project link manifest"]
      end
    ),
    known_limits: [
      "Windows wrapper execution requires verification on a Windows host."
    ]
  }' > "${OUTPUT}"
