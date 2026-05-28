#!/usr/bin/env bash
set -euo pipefail

if [ $# -ne 1 ]; then
  echo "Usage: $0 <release-zip>" >&2
  exit 2
fi

ZIP_PATH="$1"
if [ ! -f "${ZIP_PATH}" ]; then
  echo "[verify_release_package] zip not found: ${ZIP_PATH}" >&2
  exit 2
fi

unzip -Z1 "${ZIP_PATH}" | while IFS= read -r entry; do
  case "${entry}" in
    /*|../*|*/../*)
      echo "[verify_release_package] unsafe zip entry: ${entry}" >&2
      exit 2
      ;;
  esac
done

WORK_DIR="$(mktemp -d)"
cleanup() {
  rm -rf "${WORK_DIR}"
}
trap cleanup EXIT

unzip -q "${ZIP_PATH}" -d "${WORK_DIR}"

MANIFEST="${WORK_DIR}/uecommandforge-manifest.json"
test -f "${MANIFEST}"

jq -e '
  (.version | length > 0)
  and .engine_version == "UE5.7"
  and (.release_channel | length > 0)
  and (.plugin_files | type == "array")
  and (.tool_files | type == "array")
  and (.spec_files | type == "array")
  and ((.plugin_files | length) + (.tool_files | length) + (.spec_files | length) > 0)
  and (.install_commands | type == "array" and length > 0)
  and (.post_install_checks | type == "array" and length > 0)
' "${MANIFEST}" >/dev/null

jq -r '.plugin_files[]' "${MANIFEST}" | while IFS= read -r path; do
  test -f "${WORK_DIR}/${path}"
done

jq -r '.tool_files[]' "${MANIFEST}" | while IFS= read -r path; do
  test -f "${WORK_DIR}/${path}"
done

jq -r '.spec_files[]' "${MANIFEST}" | while IFS= read -r path; do
  test -f "${WORK_DIR}/${path}"
done

test -f "${WORK_DIR}/install.md"
test -f "${WORK_DIR}/release-notes.md"
