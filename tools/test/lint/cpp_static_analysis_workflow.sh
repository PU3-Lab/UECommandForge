#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
WORKFLOW="${REPO_ROOT}/.github/workflows/cpp-static-analysis.yml"

test -f "${WORKFLOW}"
grep -q 'workflow_dispatch:' "${WORKFLOW}"
grep -q 'run_clang_tidy:' "${WORKFLOW}"
grep -q 'type: boolean' "${WORKFLOW}"
grep -q 'windows-2025-vs2026' "${WORKFLOW}"
grep -q 'actions/checkout@v6' "${WORKFLOW}"
grep -q 'tools/lint/generate_compile_commands.sh' "${WORKFLOW}"
grep -q 'RUN_CLANG_TIDY: "1"' "${WORKFLOW}"
grep -q 'tools/lint/cpp_static_analysis.sh --clang-tidy-only' "${WORKFLOW}"
grep -q '.\\tools\\lint\\cpp_static_analysis.ps1 -ClangTidyOnly' "${WORKFLOW}"
grep -q 'tools\\lint\\cpp_static_analysis.bat --clang-tidy-only' "${WORKFLOW}"
