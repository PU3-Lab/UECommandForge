#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
PS1="${REPO_ROOT}/tools/lint/cpp_static_analysis.ps1"

test -f "${PS1}"
grep -q 'param(' "${PS1}"
grep -q 'CheckTools' "${PS1}"
grep -q 'CppcheckOnly' "${PS1}"
grep -q 'ClangTidyOnly' "${PS1}"
grep -q 'C:\\Program Files\\LLVM\\bin\\clang-tidy.exe' "${PS1}"
grep -q 'C:\\Program Files\\Microsoft Visual Studio\\2026\\Enterprise\\VC\\Tools\\Llvm\\x64\\bin\\clang-tidy.exe' "${PS1}"
grep -q 'C:\\Program Files\\Cppcheck\\cppcheck.exe' "${PS1}"
grep -q 'COMPILE_COMMANDS_DIR' "${PS1}"
grep -q 'RUN_CLANG_TIDY' "${PS1}"
grep -q 'CLANG_TIDY_CHECKS' "${PS1}"
