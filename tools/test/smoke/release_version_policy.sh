#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
VERSION="$(jq -r '.VersionName' "${REPO_ROOT}/sample/Plugins/UECommandForge/UECommandForge.uplugin")"

if [ "${VERSION}" != "0.8.0" ]; then
  echo "Phase 8 release VersionName must be 0.8.0, got ${VERSION}" >&2
  exit 1
fi

test -f "${REPO_ROOT}/CHANGELOG.md"
grep -q '^## 0\.8\.0 - 2026-05-28$' "${REPO_ROOT}/CHANGELOG.md"
grep -q 'UECommandForge-0.8.0-Tools.zip' "${REPO_ROOT}/README.md"
grep -q 'UECommandForge-0.8.0-UE5.7-Mac.zip' "${REPO_ROOT}/README.md"

if rg -n '0\.1\.0' \
  "${REPO_ROOT}/README.md" \
  "${REPO_ROOT}/docs/superpowers/plans/ucf-phase8-review-eval-report.md" \
  >/dev/null; then
  echo "release-facing docs must not advertise 0.1.0 after 0.8.0 release promotion" >&2
  exit 1
fi
