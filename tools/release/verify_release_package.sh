#!/usr/bin/env bash
set -euo pipefail

if [ $# -lt 1 ] || [ $# -gt 2 ]; then
  echo "Usage: $0 <release-zip> [checksums.txt]" >&2
  exit 2
fi

ZIP_PATH="$1"
if [ ! -f "${ZIP_PATH}" ]; then
  echo "[verify_release_package] zip not found: ${ZIP_PATH}" >&2
  exit 2
fi

unzip -Z1 "${ZIP_PATH}" | while IFS= read -r entry; do
  case "${entry}" in
    ''|..|*/..|/*|../*|*/../*|*\\*|[A-Za-z]:*|//*)
      echo "[verify_release_package] unsafe zip entry: ${entry}" >&2
      exit 2
      ;;
  esac
done

if unzip -Z -l "${ZIP_PATH}" | awk '$1 ~ /^l/ { print; found = 1 } END { exit found ? 1 : 0 }'; then
  :
else
  echo "[verify_release_package] symlink entries are not allowed" >&2
  exit 2
fi

WORK_DIR="$(mktemp -d)"
LIST_DIR="$(mktemp -d)"
cleanup() {
  rm -rf "${WORK_DIR}"
  rm -rf "${LIST_DIR}"
}
trap cleanup EXIT

unzip -q "${ZIP_PATH}" -d "${WORK_DIR}"

MANIFEST="${WORK_DIR}/uecommandforge-manifest.json"
test -f "${MANIFEST}"

jq -e '
  . as $manifest
  |
  (.version | length > 0)
  and .engine_version == "UE5.7"
  and (.release_channel | length > 0)
  and (.plugin_files | type == "array")
  and (.tool_files | type == "array")
  and (.spec_files | type == "array")
  and ((.plugin_files | length) + (.tool_files | length) + (.spec_files | length) > 0)
  and (.checksums | type == "object")
  and ([.plugin_files[], .tool_files[], .spec_files[], "install.md", "release-notes.md", "validation-report.json"] | all(. as $path | ($manifest.checksums[$path] | type == "string")))
  and (.install_commands | type == "array" and all(type == "string"))
  and (.post_install_checks | type == "array" and length > 0)
' "${MANIFEST}" >/dev/null

jq -r '.plugin_files[]' "${MANIFEST}" | while IFS= read -r path; do
  case "${path}" in
    ''|..|*/..|/*|../*|*/../*|*\\*|[A-Za-z]:*|//*)
      echo "[verify_release_package] unsafe manifest path: ${path}" >&2
      exit 2
      ;;
  esac
  test -f "${WORK_DIR}/${path}"
done

jq -r '.tool_files[]' "${MANIFEST}" | while IFS= read -r path; do
  case "${path}" in
    ''|..|*/..|/*|../*|*/../*|*\\*|[A-Za-z]:*|//*)
      echo "[verify_release_package] unsafe manifest path: ${path}" >&2
      exit 2
      ;;
  esac
  test -f "${WORK_DIR}/${path}"
done

jq -r '.spec_files[]' "${MANIFEST}" | while IFS= read -r path; do
  case "${path}" in
    ''|..|*/..|/*|../*|*/../*|*\\*|[A-Za-z]:*|//*)
      echo "[verify_release_package] unsafe manifest path: ${path}" >&2
      exit 2
      ;;
  esac
  test -f "${WORK_DIR}/${path}"
done

EXPECTED_FILES="${LIST_DIR}/expected-files.txt"
ACTUAL_FILES="${LIST_DIR}/actual-files.txt"
CHECKSUM_FILES="${LIST_DIR}/checksum-files.txt"

jq -r '.plugin_files[], .tool_files[], .spec_files[], "install.md", "release-notes.md", "validation-report.json"' "${MANIFEST}" \
  | sort > "${EXPECTED_FILES}"

(
  cd "${WORK_DIR}"
  find . -type f \
    ! -path './uecommandforge-manifest.json' \
    | sed 's#^\./##' \
    | sort
) > "${ACTUAL_FILES}"

jq -r '.checksums | keys[]' "${MANIFEST}" | sort > "${CHECKSUM_FILES}"

if ! cmp -s "${EXPECTED_FILES}" "${ACTUAL_FILES}"; then
  echo "[verify_release_package] manifest file list does not match zip contents" >&2
  diff -u "${EXPECTED_FILES}" "${ACTUAL_FILES}" >&2 || true
  exit 2
fi

if ! cmp -s "${EXPECTED_FILES}" "${CHECKSUM_FILES}"; then
  echo "[verify_release_package] checksum list does not match manifest file list" >&2
  diff -u "${EXPECTED_FILES}" "${CHECKSUM_FILES}" >&2 || true
  exit 2
fi

jq -r '.checksums | to_entries[] | [.key, .value] | @tsv' "${MANIFEST}" | while IFS=$'\t' read -r path expected; do
  case "${path}" in
    ''|..|*/..|/*|../*|*/../*|*\\*|[A-Za-z]:*|//*)
      echo "[verify_release_package] unsafe checksum path: ${path}" >&2
      exit 2
      ;;
  esac
  actual="$(shasum -a 256 "${WORK_DIR}/${path}" | awk '{ print $1 }')"
  test "${actual}" = "${expected}"
done

if [ $# -eq 2 ]; then
  CHECKSUMS_PATH="$2"
  if [ ! -f "${CHECKSUMS_PATH}" ]; then
    echo "[verify_release_package] checksums file not found: ${CHECKSUMS_PATH}" >&2
    exit 2
  fi
  checksum_line_count="$(awk 'NF { count++ } END { print count + 0 }' "${CHECKSUMS_PATH}")"
  if [ "${checksum_line_count}" -ne 1 ]; then
    echo "[verify_release_package] checksums file must contain exactly one entry" >&2
    exit 2
  fi

  read -r expected_zip_hash checksum_name checksum_extra < "${CHECKSUMS_PATH}"
  if [ -n "${checksum_extra:-}" ]; then
    echo "[verify_release_package] checksums file entry has too many fields" >&2
    exit 2
  fi
  checksum_name="${checksum_name#\*}"
  case "${checksum_name}" in
    ''|..|*/..|/*|../*|*/../*|*/*|*\\*|[A-Za-z]:*|//*)
      echo "[verify_release_package] unsafe checksums filename: ${checksum_name}" >&2
      exit 2
      ;;
  esac
  if ! [[ "${expected_zip_hash}" =~ ^[0-9a-fA-F]{64}$ ]]; then
    echo "[verify_release_package] checksums file hash is not SHA256" >&2
    exit 2
  fi
  if [ "${checksum_name}" != "$(basename "${ZIP_PATH}")" ]; then
    echo "[verify_release_package] checksums file does not reference release zip: ${checksum_name}" >&2
    exit 2
  fi

  expected_zip_hash="$(printf '%s' "${expected_zip_hash}" | tr '[:upper:]' '[:lower:]')"
  actual_zip_hash="$(shasum -a 256 "${ZIP_PATH}" | awk '{ print $1 }')"
  if [ "${actual_zip_hash}" != "${expected_zip_hash}" ]; then
    echo "[verify_release_package] release zip checksum mismatch" >&2
    exit 2
  fi
fi

test -f "${WORK_DIR}/install.md"
test -f "${WORK_DIR}/release-notes.md"
test -f "${WORK_DIR}/validation-report.json"
