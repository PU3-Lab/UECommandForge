#!/usr/bin/env bash
set -euo pipefail

PACKAGE_ROOT=""
VERSION=""
CHANNEL="local"
OUTPUT=""
ENGINE_VERSION="UE5.7"

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
    ! -name 'uecommandforge-manifest.json' \
    ! -name 'install.md' \
    ! -name 'release-notes.md' \
    | sed 's#^\./##' \
    | sort \
    | jq -R -s 'split("\n")[:-1]'
)"
tool_files="$(files_json "${PACKAGE_ROOT}" tools)"
spec_files="$(files_json "${PACKAGE_ROOT}" specs)"

jq -n \
  --arg version "${VERSION}" \
  --arg engine_version "${ENGINE_VERSION}" \
  --arg release_channel "${CHANNEL}" \
  --argjson plugin_files "${plugin_files}" \
  --argjson tool_files "${tool_files}" \
  --argjson spec_files "${spec_files}" \
  '{
    version: $version,
    engine_version: $engine_version,
    release_channel: $release_channel,
    plugin_files: $plugin_files,
    tool_files: $tool_files,
    spec_files: $spec_files,
    checksums: {},
    install_commands: [
      "install-uecommandforge.sh --project <path-to-uproject>",
      "tools/release/install_local.sh --project <path-to-uproject>"
    ],
    post_install_checks: [
      "Hello commandlet",
      "Codex tools path",
      "project link manifest"
    ],
    known_limits: [
      "Tools package does not include compiled plugin binaries.",
      "Install scripts are added in the next Task 6 step."
    ]
  }' > "${OUTPUT}"
