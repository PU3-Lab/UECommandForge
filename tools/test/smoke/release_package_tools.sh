#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
# shellcheck disable=SC1091
source "${REPO_ROOT}/tools/release/common.sh"
WORK_DIR="${REPO_ROOT}/sample/Saved/CodexReports/ReleasePackageTools"
VERSION="0.8.0-test"
SOURCE_REPARSE_LINK="${REPO_ROOT}/tools/test/smoke/.release_package_reparse_fixture"
OUTPUT_REPARSE_LINK=""
PACKAGE_REPARSE_LINK=""
PACKAGE_CHILD_REPARSE_LINK=""
ZIP_DIR_REPARSE_LINK=""
RELATIVE_REPARSE_LINK=""

remove_reparse_fixture() {
  local path="$1"
  if [ -z "${path}" ]; then
    return 0
  fi

  if uecf_is_windows_bash && command -v cygpath >/dev/null 2>&1 && command -v powershell.exe >/dev/null 2>&1; then
    local windows_link
    windows_link="$(cygpath -w "${path}")"
    powershell.exe -NoProfile -ExecutionPolicy Bypass -Command \
      '& {
        param([string]$Path)
        $ErrorActionPreference = "Stop"
        if (Test-Path -LiteralPath $Path) {
          $Item = Get-Item -LiteralPath $Path -Force
          if (($Item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
            $Item.Delete()
          } else {
            Remove-Item -LiteralPath $Path -Recurse -Force
          }
        }
      }' \
      "${windows_link}" >/dev/null
  else
    rm -f "${path}"
    rmdir "${path}" 2>/dev/null || true
  fi
}

create_reparse_fixture() {
  local link="$1"
  local target="$2"
  remove_reparse_fixture "${link}"

  if uecf_is_windows_bash; then
    local windows_link
    local windows_target
    windows_link="$(cygpath -w "${link}")"
    windows_target="$(cygpath -w "${target}")"
    powershell.exe -NoProfile -ExecutionPolicy Bypass -Command \
      '& {
        param([string]$Link, [string]$Target)
        $ErrorActionPreference = "Stop"
        New-Item -ItemType Junction -Path $Link -Target $Target -Force | Out-Null
      }' \
      "${windows_link}" "${windows_target}" >/dev/null
  else
    ln -s "${target}" "${link}"
  fi

  uecf_path_is_reparse_point "${link}"
}

cleanup() {
  remove_reparse_fixture "${SOURCE_REPARSE_LINK}"
  remove_reparse_fixture "${OUTPUT_REPARSE_LINK}"
  remove_reparse_fixture "${PACKAGE_REPARSE_LINK}"
  remove_reparse_fixture "${PACKAGE_CHILD_REPARSE_LINK}"
  remove_reparse_fixture "${ZIP_DIR_REPARSE_LINK}"
  remove_reparse_fixture "${RELATIVE_REPARSE_LINK}"
}
trap cleanup EXIT

rm -rf "${WORK_DIR}"
mkdir -p "${WORK_DIR}"

"${REPO_ROOT}/tools/release/package_tools.sh" \
  --version "${VERSION}" \
  --channel local-smoke \
  --out-dir "${WORK_DIR}"

TOOLS_ZIP="${WORK_DIR}/UECommandForge-${VERSION}-Tools.zip"
CHECKSUMS="${WORK_DIR}/checksums.txt"
ZIP_LIST="${WORK_DIR}/zip-files.txt"

test -f "${TOOLS_ZIP}"
test -f "${CHECKSUMS}"

unzip -Z1 "${TOOLS_ZIP}" > "${ZIP_LIST}"
grep -q '^tools/ue/run_commandlet.sh$' "${ZIP_LIST}"
grep -q '^tools/ue/set_blueprint_defaults.sh$' "${ZIP_LIST}"
grep -q '^tools/ue/set_blueprint_defaults.bat$' "${ZIP_LIST}"
grep -q '^tools/test/smoke/mid_real_project_flow.sh$' "${ZIP_LIST}"
grep -q '^tools/release/package_tools.sh$' "${ZIP_LIST}"
grep -q '^specs/policies/assets.policy.json$' "${ZIP_LIST}"
grep -q '^specs/codex/unreal-automation-agents.md$' "${ZIP_LIST}"
grep -q '^specs/examples/blueprint_defaults.json$' "${ZIP_LIST}"
grep -q '^install-uecommandforge.sh$' "${ZIP_LIST}"
grep -q '^install-uecommandforge.bat$' "${ZIP_LIST}"
grep -q '^install-uecommandforge.ps1$' "${ZIP_LIST}"
grep -q '^uecommandforge-manifest.json$' "${ZIP_LIST}"
grep -q '^validation-report.json$' "${ZIP_LIST}"
unzip -Z -v "${TOOLS_ZIP}" install.md release-notes.md validation-report.json uecommandforge-manifest.json \
  | grep -E 'Unix file attributes' \
  | awk '{ print $4 }' \
  | while IFS= read -r mode; do
    mode="${mode#(}"
    mode="${mode%,}"
    if [ "${mode}" != "100644" ]; then
      echo "release package metadata should be 100644, got ${mode}" >&2
      exit 1
    fi
  done

unzip -p "${TOOLS_ZIP}" uecommandforge-manifest.json | jq -e \
  --arg version "${VERSION}" \
  '.version == $version
   and .engine_version == "UE5.7"
   and .release_channel == "local-smoke"
   and (.tool_files | index("tools/ue/run_commandlet.sh"))
   and (.tool_files | index("tools/ue/set_blueprint_defaults.sh"))
   and (.tool_files | index("tools/ue/set_blueprint_defaults.bat"))
   and (.tool_files | index("tools/test/smoke/mid_real_project_flow.sh"))
   and (.spec_files | index("specs/policies/assets.policy.json"))
   and (.spec_files | index("specs/codex/unreal-automation-agents.md"))
   and (.spec_files | index("specs/examples/blueprint_defaults.json"))
   and (.install_commands | index("./install-uecommandforge.sh"))
   and (.install_commands | index("install-uecommandforge.bat"))
   and (.install_commands | index("install-uecommandforge.ps1"))
   and (.checksums["tools/ue/run_commandlet.sh"] | type == "string")
   and (.checksums["tools/ue/set_blueprint_defaults.sh"] | type == "string")
   and (.checksums["tools/ue/set_blueprint_defaults.bat"] | type == "string")
   and (.checksums["specs/examples/blueprint_defaults.json"] | type == "string")
   and (.checksums["install-uecommandforge.sh"] | type == "string")
   and (.checksums["install.md"] | type == "string")
   and (.checksums["release-notes.md"] | type == "string")
   and (.checksums["validation-report.json"] | type == "string")
   and (.post_install_checks[] | select(. == "Hello commandlet"))
   and (.post_install_checks[] | select(. == "Blueprint defaults wrapper"))' >/dev/null

unzip -p "${TOOLS_ZIP}" validation-report.json | jq -e \
  --arg version "${VERSION}" \
  '.version == $version
   and .package_type == "tools"
   and .status == "pass"
   and (.checks[] | select(.name == "generated tools package"))
   and (.checks[] | select(.name == "blueprint defaults wrapper included"
      and (.required_files | index("tools/ue/set_blueprint_defaults.sh"))
      and (.required_files | index("tools/ue/set_blueprint_defaults.bat"))
      and (.required_files | index("specs/examples/blueprint_defaults.json"))))' >/dev/null

"${REPO_ROOT}/tools/release/verify_release_package.sh" "${TOOLS_ZIP}" "${CHECKSUMS}"

grep -q "UECommandForge-${VERSION}-Tools.zip" "${CHECKSUMS}"

if "${REPO_ROOT}/tools/release/package_tools.sh" \
  --version '../bad' \
  --out-dir "${WORK_DIR}/bad" >/dev/null 2>&1; then
  echo "unsafe version should fail" >&2
  exit 1
fi

SOURCE_REPARSE_TARGET="${WORK_DIR}/source-reparse-target"
SOURCE_REPARSE_OUT="${WORK_DIR}/source-reparse"
SOURCE_REPARSE_SENTINEL="${SOURCE_REPARSE_OUT}/UECommandForge-${VERSION}-Tools/sentinel.txt"
mkdir -p "${SOURCE_REPARSE_TARGET}"
mkdir -p "$(dirname "${SOURCE_REPARSE_SENTINEL}")"
printf 'preserve existing output\n' > "${SOURCE_REPARSE_SENTINEL}"
if ! create_reparse_fixture "${SOURCE_REPARSE_LINK}" "${SOURCE_REPARSE_TARGET}"; then
  echo "could not create package input reparse fixture" >&2
  exit 1
fi
if "${REPO_ROOT}/tools/release/package_tools.sh" \
  --version "${VERSION}" \
  --channel source-reparse \
  --out-dir "${SOURCE_REPARSE_OUT}" >/dev/null 2>&1; then
  echo "package tools should reject reparse points in source tools tree" >&2
  exit 1
fi
grep -q 'preserve existing output' "${SOURCE_REPARSE_SENTINEL}"
remove_reparse_fixture "${SOURCE_REPARSE_LINK}"

OUTPUT_REPARSE_TARGET="${WORK_DIR}/output-reparse-target"
OUTPUT_REPARSE_LINK="${WORK_DIR}/output-reparse-link"
mkdir -p "${OUTPUT_REPARSE_TARGET}"
if ! create_reparse_fixture "${OUTPUT_REPARSE_LINK}" "${OUTPUT_REPARSE_TARGET}"; then
  echo "could not create package output reparse fixture" >&2
  exit 1
fi
if "${REPO_ROOT}/tools/release/package_tools.sh" \
  --version "${VERSION}" \
  --channel output-reparse \
  --out-dir "${OUTPUT_REPARSE_LINK}" >/dev/null 2>&1; then
  echo "package tools should reject reparse output directory" >&2
  exit 1
fi
test ! -e "${OUTPUT_REPARSE_TARGET}/UECommandForge-${VERSION}-Tools"
remove_reparse_fixture "${OUTPUT_REPARSE_LINK}"
OUTPUT_REPARSE_LINK=""

PACKAGE_REPARSE_OUT="${WORK_DIR}/package-reparse-out"
PACKAGE_REPARSE_TARGET="${WORK_DIR}/package-reparse-target"
PACKAGE_REPARSE_LINK="${PACKAGE_REPARSE_OUT}/UECommandForge-${VERSION}-Tools"
mkdir -p "${PACKAGE_REPARSE_OUT}" "${PACKAGE_REPARSE_TARGET}"
if ! create_reparse_fixture "${PACKAGE_REPARSE_LINK}" "${PACKAGE_REPARSE_TARGET}"; then
  echo "could not create existing package dir reparse fixture" >&2
  exit 1
fi
if "${REPO_ROOT}/tools/release/package_tools.sh" \
  --version "${VERSION}" \
  --channel package-reparse \
  --out-dir "${PACKAGE_REPARSE_OUT}" >/dev/null 2>&1; then
  echo "package tools should reject existing reparse package directory" >&2
  exit 1
fi
test ! -e "${PACKAGE_REPARSE_TARGET}/tools"
remove_reparse_fixture "${PACKAGE_REPARSE_LINK}"
PACKAGE_REPARSE_LINK=""

PACKAGE_CHILD_REPARSE_OUT="${WORK_DIR}/package-child-reparse-out"
PACKAGE_CHILD_REPARSE_DIR="${PACKAGE_CHILD_REPARSE_OUT}/UECommandForge-${VERSION}-Tools"
PACKAGE_CHILD_REPARSE_TARGET="${WORK_DIR}/package-child-reparse-target"
PACKAGE_CHILD_REPARSE_LINK="${PACKAGE_CHILD_REPARSE_DIR}/tools"
mkdir -p "${PACKAGE_CHILD_REPARSE_DIR}" "${PACKAGE_CHILD_REPARSE_TARGET}"
printf 'preserve package child target\n' > "${PACKAGE_CHILD_REPARSE_TARGET}/sentinel.txt"
if ! create_reparse_fixture "${PACKAGE_CHILD_REPARSE_LINK}" "${PACKAGE_CHILD_REPARSE_TARGET}"; then
  echo "could not create existing package child reparse fixture" >&2
  exit 1
fi
if "${REPO_ROOT}/tools/release/package_tools.sh" \
  --version "${VERSION}" \
  --channel package-child-reparse \
  --out-dir "${PACKAGE_CHILD_REPARSE_OUT}" >/dev/null 2>&1; then
  echo "package tools should reject existing reparse child in package directory" >&2
  exit 1
fi
grep -q 'preserve package child target' "${PACKAGE_CHILD_REPARSE_TARGET}/sentinel.txt"
remove_reparse_fixture "${PACKAGE_CHILD_REPARSE_LINK}"
PACKAGE_CHILD_REPARSE_LINK=""

ZIP_DIR_REPARSE_OUT="${WORK_DIR}/zip-dir-reparse-out"
ZIP_DIR_REPARSE_PATH="${ZIP_DIR_REPARSE_OUT}/UECommandForge-${VERSION}-Tools.zip"
ZIP_DIR_REPARSE_TARGET="${WORK_DIR}/zip-dir-reparse-target"
ZIP_DIR_REPARSE_LINK="${ZIP_DIR_REPARSE_PATH}/child"
mkdir -p "${ZIP_DIR_REPARSE_PATH}" "${ZIP_DIR_REPARSE_TARGET}"
printf 'preserve zip path child target\n' > "${ZIP_DIR_REPARSE_TARGET}/sentinel.txt"
if ! create_reparse_fixture "${ZIP_DIR_REPARSE_LINK}" "${ZIP_DIR_REPARSE_TARGET}"; then
  echo "could not create zip path directory reparse fixture" >&2
  exit 1
fi
if "${REPO_ROOT}/tools/release/package_tools.sh" \
  --version "${VERSION}" \
  --channel zip-dir-reparse \
  --out-dir "${ZIP_DIR_REPARSE_OUT}" >/dev/null 2>&1; then
  echo "package tools should reject directory at zip output path" >&2
  exit 1
fi
grep -q 'preserve zip path child target' "${ZIP_DIR_REPARSE_TARGET}/sentinel.txt"
remove_reparse_fixture "${ZIP_DIR_REPARSE_LINK}"
ZIP_DIR_REPARSE_LINK=""

RELATIVE_REPARSE_TARGET="${WORK_DIR}/relative-reparse-target"
RELATIVE_REPARSE_LINK="${WORK_DIR}/relative-reparse-link"
mkdir -p "${RELATIVE_REPARSE_TARGET}"
if ! create_reparse_fixture "${RELATIVE_REPARSE_LINK}" "${RELATIVE_REPARSE_TARGET}"; then
  echo "could not create relative cwd reparse fixture" >&2
  exit 1
fi
(
  cd "${RELATIVE_REPARSE_LINK}"
  if "${REPO_ROOT}/tools/release/package_tools.sh" \
    --version "${VERSION}" \
    --channel relative-reparse \
    --out-dir relative-output >/dev/null 2>&1; then
    echo "package tools should reject relative output below reparse cwd" >&2
    exit 1
  fi
)
test ! -e "${RELATIVE_REPARSE_TARGET}/relative-output"
remove_reparse_fixture "${RELATIVE_REPARSE_LINK}"
RELATIVE_REPARSE_LINK=""

CHECKSUM_HARDLINK_OUT="${WORK_DIR}/checksum-hardlink"
CHECKSUM_HARDLINK_TARGET="${WORK_DIR}/checksum-hardlink-target.txt"
mkdir -p "${CHECKSUM_HARDLINK_OUT}"
printf 'preserve checksum hardlink target\n' > "${CHECKSUM_HARDLINK_TARGET}"
ln "${CHECKSUM_HARDLINK_TARGET}" "${CHECKSUM_HARDLINK_OUT}/checksums.txt"
"${REPO_ROOT}/tools/release/package_tools.sh" \
  --version "${VERSION}" \
  --channel checksum-hardlink \
  --out-dir "${CHECKSUM_HARDLINK_OUT}" >/dev/null
grep -q 'preserve checksum hardlink target' "${CHECKSUM_HARDLINK_TARGET}"
grep -q "UECommandForge-${VERSION}-Tools.zip" "${CHECKSUM_HARDLINK_OUT}/checksums.txt"

CHECKSUM_FAILURE_OUT="${WORK_DIR}/checksum-failure"
mkdir -p "${CHECKSUM_FAILURE_OUT}"
printf 'preserve failed checksum output\n' > "${CHECKSUM_FAILURE_OUT}/checksums.txt"
set +e
bash -c \
  'set -euo pipefail; source "$1"; uecf_write_checksums_file "$2" "$3" package_tools_smoke' \
  _ \
  "${REPO_ROOT}/tools/release/common.sh" \
  "${CHECKSUM_FAILURE_OUT}/missing.zip" \
  "${CHECKSUM_FAILURE_OUT}/checksums.txt" >/dev/null 2>&1
CHECKSUM_FAILURE_STATUS=$?
set -e
if [ "${CHECKSUM_FAILURE_STATUS}" -eq 0 ]; then
  echo "missing zip checksum write should fail" >&2
  exit 1
fi
grep -q 'preserve failed checksum output' "${CHECKSUM_FAILURE_OUT}/checksums.txt"

MANIFEST_OUTPUT_ROOT="${WORK_DIR}/manifest-output-root"
MANIFEST_OUTPUT_TARGET="${WORK_DIR}/manifest-output-target.json"
MANIFEST_OUTPUT_LINK="${WORK_DIR}/manifest-output-link.json"
mkdir -p "${MANIFEST_OUTPUT_ROOT}/tools" "${MANIFEST_OUTPUT_ROOT}/specs"
printf '# install\n' > "${MANIFEST_OUTPUT_ROOT}/install.md"
printf '# notes\n' > "${MANIFEST_OUTPUT_ROOT}/release-notes.md"
printf '{"status":"pass"}\n' > "${MANIFEST_OUTPUT_ROOT}/validation-report.json"
printf 'tool\n' > "${MANIFEST_OUTPUT_ROOT}/tools/tool.txt"
printf 'spec\n' > "${MANIFEST_OUTPUT_ROOT}/specs/spec.txt"
printf 'preserve manifest target\n' > "${MANIFEST_OUTPUT_TARGET}"
ln -s "${MANIFEST_OUTPUT_TARGET}" "${MANIFEST_OUTPUT_LINK}"
if "${REPO_ROOT}/tools/release/write_manifest.sh" \
  --package-root "${MANIFEST_OUTPUT_ROOT}" \
  --version "${VERSION}" \
  --channel manifest-output \
  --package-type tools \
  --output "${MANIFEST_OUTPUT_LINK}" >/dev/null 2>&1; then
  echo "write_manifest should reject symlink output path" >&2
  exit 1
fi
grep -q 'preserve manifest target' "${MANIFEST_OUTPUT_TARGET}"

BAD_ZIP_DIR="${WORK_DIR}/bad-zip"
mkdir -p "${BAD_ZIP_DIR}"
if touch "${BAD_ZIP_DIR}/bad\\entry" 2>/dev/null; then
  (
    cd "${BAD_ZIP_DIR}"
    uecf_create_zip "${BAD_ZIP_DIR}/bad-entry.zip" 'bad\entry'
  )
else
  BAD_ENTRY_ZIP="${BAD_ZIP_DIR}/bad-entry.zip"
  if command -v powershell.exe >/dev/null 2>&1 && command -v cygpath >/dev/null 2>&1; then
    BAD_ENTRY_ZIP="$(cygpath -w "${BAD_ENTRY_ZIP}")"
    powershell.exe -NoProfile -ExecutionPolicy Bypass -Command \
      '& {
        param([string]$ZipPath)
        $ErrorActionPreference = "Stop"
        Add-Type -AssemblyName System.IO.Compression
        Add-Type -AssemblyName System.IO.Compression.FileSystem
        if (Test-Path -LiteralPath $ZipPath) { Remove-Item -LiteralPath $ZipPath -Force }
        $Archive = [System.IO.Compression.ZipFile]::Open($ZipPath, [System.IO.Compression.ZipArchiveMode]::Create)
        try {
          $Entry = $Archive.CreateEntry("bad\entry")
          $Writer = New-Object System.IO.StreamWriter($Entry.Open())
          try {
            $Writer.Write("bad")
          } finally {
            $Writer.Dispose()
          }
        } finally {
          $Archive.Dispose()
        }
      }' \
      "${BAD_ENTRY_ZIP}"
  else
    echo "could not create unsafe backslash zip entry fixture" >&2
    exit 1
  fi
fi
if "${REPO_ROOT}/tools/release/verify_release_package.sh" \
  "${BAD_ZIP_DIR}/bad-entry.zip" >/dev/null 2>&1; then
  echo "unsafe zip entry should fail" >&2
  exit 1
fi

BAD_MANIFEST_DIR="${WORK_DIR}/bad-manifest"
rm -rf "${BAD_MANIFEST_DIR}"
mkdir -p "${BAD_MANIFEST_DIR}"
unzip -q "${TOOLS_ZIP}" -d "${BAD_MANIFEST_DIR}/package"
jq 'del(.checksums["install.md"])' \
  "${BAD_MANIFEST_DIR}/package/uecommandforge-manifest.json" \
  > "${BAD_MANIFEST_DIR}/package/uecommandforge-manifest.json.tmp"
mv "${BAD_MANIFEST_DIR}/package/uecommandforge-manifest.json.tmp" \
  "${BAD_MANIFEST_DIR}/package/uecommandforge-manifest.json"
(
  cd "${BAD_MANIFEST_DIR}/package"
  uecf_create_zip "${BAD_MANIFEST_DIR}/missing-checksum.zip" ./*
)
if "${REPO_ROOT}/tools/release/verify_release_package.sh" \
  "${BAD_MANIFEST_DIR}/missing-checksum.zip" >/dev/null 2>&1; then
  echo "manifest missing required checksum should fail" >&2
  exit 1
fi

rm -rf "${BAD_MANIFEST_DIR}/package"
mkdir -p "${BAD_MANIFEST_DIR}/package"
unzip -q "${TOOLS_ZIP}" -d "${BAD_MANIFEST_DIR}/package"
printf 'unexpected\n' > "${BAD_MANIFEST_DIR}/package/extra.sh"
(
  cd "${BAD_MANIFEST_DIR}/package"
  uecf_create_zip "${BAD_MANIFEST_DIR}/unlisted-file.zip" ./*
)
if "${REPO_ROOT}/tools/release/verify_release_package.sh" \
  "${BAD_MANIFEST_DIR}/unlisted-file.zip" >/dev/null 2>&1; then
  echo "unlisted zip file should fail" >&2
  exit 1
fi

rm -rf "${BAD_MANIFEST_DIR}/package"
mkdir -p "${BAD_MANIFEST_DIR}/package"
unzip -q "${TOOLS_ZIP}" -d "${BAD_MANIFEST_DIR}/package"
printf 'unexpected\n' > "${BAD_MANIFEST_DIR}/package/.expected-files.txt"
(
  cd "${BAD_MANIFEST_DIR}/package"
  uecf_create_zip "${BAD_MANIFEST_DIR}/reserved-unlisted-file.zip" .
)
if "${REPO_ROOT}/tools/release/verify_release_package.sh" \
  "${BAD_MANIFEST_DIR}/reserved-unlisted-file.zip" >/dev/null 2>&1; then
  echo "reserved unlisted zip file should fail" >&2
  exit 1
fi

rm -rf "${BAD_MANIFEST_DIR}/package"
mkdir -p "${BAD_MANIFEST_DIR}/package/tools/extra"
unzip -q "${TOOLS_ZIP}" -d "${BAD_MANIFEST_DIR}/package"
printf '{}\n' > "${BAD_MANIFEST_DIR}/package/tools/extra/uecommandforge-manifest.json"
(
  cd "${BAD_MANIFEST_DIR}/package"
  uecf_create_zip "${BAD_MANIFEST_DIR}/nested-manifest-file.zip" ./*
)
if "${REPO_ROOT}/tools/release/verify_release_package.sh" \
  "${BAD_MANIFEST_DIR}/nested-manifest-file.zip" >/dev/null 2>&1; then
  echo "nested manifest-named unlisted zip file should fail" >&2
  exit 1
fi

rm -rf "${BAD_MANIFEST_DIR}/package"
mkdir -p "${BAD_MANIFEST_DIR}/package"
printf 'x' > "${BAD_MANIFEST_DIR}/package/install.md"
printf 'x' > "${BAD_MANIFEST_DIR}/package/release-notes.md"
printf 'x' > "${BAD_MANIFEST_DIR}/package/validation-report.json"
jq -n '{
  version: "bad",
  engine_version: "UE5.7",
  release_channel: "local-smoke",
  plugin_files: [".."],
  tool_files: [],
  spec_files: [],
  checksums: {"..": "0", "install.md": "0", "release-notes.md": "0", "validation-report.json": "0"},
  install_commands: [],
  post_install_checks: ["Hello commandlet"]
}' > "${BAD_MANIFEST_DIR}/package/uecommandforge-manifest.json"
(
  cd "${BAD_MANIFEST_DIR}/package"
  uecf_create_zip "${BAD_MANIFEST_DIR}/unsafe-manifest-path.zip" ./*
)
if "${REPO_ROOT}/tools/release/verify_release_package.sh" \
  "${BAD_MANIFEST_DIR}/unsafe-manifest-path.zip" >/dev/null 2>&1; then
  echo "unsafe manifest path should fail" >&2
  exit 1
fi

BAD_CHECKSUMS="${WORK_DIR}/bad-checksums.txt"
printf '%064d  %s\n' 0 "$(basename "${TOOLS_ZIP}")" > "${BAD_CHECKSUMS}"
if "${REPO_ROOT}/tools/release/verify_release_package.sh" \
  "${TOOLS_ZIP}" "${BAD_CHECKSUMS}" >/dev/null 2>&1; then
  echo "bad checksum should fail" >&2
  exit 1
fi

SIDE_FILE="${WORK_DIR}/side.txt"
SIDE_CHECKSUMS="${WORK_DIR}/side-checksums.txt"
printf 'side\n' > "${SIDE_FILE}"
"${REPO_ROOT}/tools/release/write_checksums.sh" "${SIDE_FILE}" > "${SIDE_CHECKSUMS}"
if "${REPO_ROOT}/tools/release/verify_release_package.sh" \
  "${TOOLS_ZIP}" "${SIDE_CHECKSUMS}" >/dev/null 2>&1; then
  echo "checksum for a different file should fail" >&2
  exit 1
fi
