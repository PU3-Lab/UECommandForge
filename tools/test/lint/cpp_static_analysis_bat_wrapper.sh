#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
BAT="${REPO_ROOT}/tools/lint/cpp_static_analysis.bat"

test -f "${BAT}"
grep -q '@echo off' "${BAT}"
grep -q -- '--check-tools' "${BAT}"
grep -q -- '--cppcheck-only' "${BAT}"
grep -q -- '--clang-tidy-only' "${BAT}"
grep -q 'C:\\Program Files\\LLVM\\bin\\clang-tidy.exe' "${BAT}"
grep -q 'C:\\Program Files\\Microsoft Visual Studio\\2026\\Enterprise\\VC\\Tools\\Llvm\\x64\\bin\\clang-tidy.exe' "${BAT}"
grep -q 'C:\\Program Files\\Cppcheck\\cppcheck.exe' "${BAT}"
grep -q 'COMPILE_COMMANDS_DIR' "${BAT}"
grep -q 'RUN_CLANG_TIDY' "${BAT}"
grep -q 'CLANG_TIDY_CHECKS' "${BAT}"
