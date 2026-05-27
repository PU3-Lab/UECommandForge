#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
SH="${REPO_ROOT}/tools/lint/generate_compile_commands.sh"
PS1="${REPO_ROOT}/tools/lint/generate_compile_commands.ps1"
BAT="${REPO_ROOT}/tools/lint/generate_compile_commands.bat"

test -x "${SH}"
grep -q 'GenerateClangDatabase' "${SH}"
grep -q 'COMPILE_COMMANDS_DIR' "${SH}"
grep -q 'PROJECT_FILE' "${SH}"

test -f "${PS1}"
grep -q 'GenerateClangDatabase' "${PS1}"
grep -q 'COMPILE_COMMANDS_DIR' "${PS1}"
grep -q 'UECommandForgeSample.uproject' "${PS1}"

test -f "${BAT}"
grep -q '@echo off' "${BAT}"
grep -q 'GenerateClangDatabase' "${BAT}"
grep -q 'COMPILE_COMMANDS_DIR' "${BAT}"
grep -q 'UECommandForgeSample.uproject' "${BAT}"
