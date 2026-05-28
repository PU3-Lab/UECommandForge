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

tool_files="$(
  cd "${PACKAGE_ROOT}"
  find tools -type f | sort | jq -R -s 'split("\n")[:-1]'
)"
spec_files="$(
  cd "${PACKAGE_ROOT}"
  find specs -type f | sort | jq -R -s 'split("\n")[:-1]'
)"

jq -n \
  --arg version "${VERSION}" \
  --arg engine_version "${ENGINE_VERSION}" \
  --arg release_channel "${CHANNEL}" \
  --argjson tool_files "${tool_files}" \
  --argjson spec_files "${spec_files}" \
  '{
    version: $version,
    engine_version: $engine_version,
    release_channel: $release_channel,
    plugin_files: [],
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
