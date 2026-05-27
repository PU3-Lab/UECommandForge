#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
SOURCE_ROOT="${REPO_ROOT}/sample/Plugins/UECommandForge/Source"

detect_platform() {
  if [ -n "${STATIC_ANALYSIS_PLATFORM:-}" ]; then
    echo "${STATIC_ANALYSIS_PLATFORM}"
    return 0
  fi

  case "$(uname -s)" in
    Darwin) echo "macos" ;;
    MINGW*|MSYS*|CYGWIN*) echo "windows" ;;
    Linux) echo "linux" ;;
    *) echo "unknown" ;;
  esac
}

first_executable() {
  for candidate in "$@"; do
    if [ -n "${candidate}" ] && [ -x "${candidate}" ]; then
      echo "${candidate}"
      return 0
    fi
  done
  return 1
}

path_command() {
  command -v "$1" 2>/dev/null || true
}

find_clang_tidy() {
  local platform="$1"
  local path_candidate
  path_candidate="$(path_command clang-tidy)"

  if [ "${platform}" = "windows" ]; then
    first_executable \
      "${CLANG_TIDY:-}" \
      "${path_candidate}" \
      "$(path_command clang-tidy.exe)" \
      "/c/Program Files/LLVM/bin/clang-tidy.exe" \
      "/c/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/Llvm/x64/bin/clang-tidy.exe" \
      "/c/Program Files/Microsoft Visual Studio/2022/Professional/VC/Tools/Llvm/x64/bin/clang-tidy.exe" \
      "/c/Program Files/Microsoft Visual Studio/2022/Enterprise/VC/Tools/Llvm/x64/bin/clang-tidy.exe"
  else
    first_executable \
      "${CLANG_TIDY:-}" \
      "${path_candidate}" \
      "/opt/homebrew/opt/llvm/bin/clang-tidy" \
      "/usr/local/opt/llvm/bin/clang-tidy"
  fi
}

find_cppcheck() {
  local platform="$1"
  local path_candidate
  path_candidate="$(path_command cppcheck)"

  if [ "${platform}" = "windows" ]; then
    first_executable \
      "${CPPCHECK:-}" \
      "$(path_command cppcheck.exe)" \
      "${path_candidate}" \
      "/c/Program Files/Cppcheck/cppcheck.exe"
  else
    first_executable "${CPPCHECK:-}" "${path_candidate}"
  fi
}

usage() {
  cat <<USAGE
Usage: tools/lint/cpp_static_analysis.sh [--check-tools] [--cppcheck-only] [--clang-tidy-only]
USAGE
}

MODE="run"
case "${1:-}" in
  --check-tools) MODE="check-tools" ;;
  --cppcheck-only) MODE="cppcheck-only" ;;
  --clang-tidy-only) MODE="clang-tidy-only" ;;
  -h|--help) usage; exit 0 ;;
  "") ;;
  *) usage >&2; exit 2 ;;
esac

PLATFORM="$(detect_platform)"
CLANG_TIDY_BIN="$(find_clang_tidy "${PLATFORM}" || true)"
CPPCHECK_BIN="$(find_cppcheck "${PLATFORM}" || true)"

if [ "${MODE}" = "check-tools" ]; then
  echo "platform: ${PLATFORM}"
  echo "clang-tidy: ${CLANG_TIDY_BIN:-not found}"
  echo "cppcheck: ${CPPCHECK_BIN:-not found}"
  [ -n "${CLANG_TIDY_BIN}" ] && [ -n "${CPPCHECK_BIN}" ]
  exit $?
fi

CPP_FILES=()
while IFS= read -r cpp_file; do
  CPP_FILES+=("${cpp_file}")
done < <(find "${SOURCE_ROOT}" -type f -name '*.cpp' | sort)

if [ "${MODE}" != "clang-tidy-only" ]; then
  if [ -z "${CPPCHECK_BIN}" ]; then
    echo "[lint] cppcheck not found. Install it or set CPPCHECK." >&2
    exit 127
  fi
  "${CPPCHECK_BIN}" \
    --enable=warning,style,performance,portability \
    --inline-suppr \
    --std=c++20 \
    --suppress=missingIncludeSystem \
    --suppress=unknownMacro \
    --suppress=syntaxError \
    "${CPP_FILES[@]}"
fi

if [ "${MODE}" = "clang-tidy-only" ] || [ "${RUN_CLANG_TIDY:-0}" = "1" ]; then
  if [ -z "${CLANG_TIDY_BIN}" ]; then
    echo "[lint] clang-tidy not found. Install LLVM or set CLANG_TIDY." >&2
    exit 127
  fi

  COMPILE_COMMANDS_DIR="${COMPILE_COMMANDS_DIR:-${REPO_ROOT}}"
  if [ ! -f "${COMPILE_COMMANDS_DIR}/compile_commands.json" ]; then
    echo "[lint] compile_commands.json not found in ${COMPILE_COMMANDS_DIR}; skipping clang-tidy."
    echo "[lint] Generate one with UnrealBuildTool or set COMPILE_COMMANDS_DIR."
    exit 0
  fi

  CLANG_TIDY_CHECKS="${CLANG_TIDY_CHECKS:-clang-analyzer-*}"
  "${CLANG_TIDY_BIN}" -checks="${CLANG_TIDY_CHECKS}" -p "${COMPILE_COMMANDS_DIR}" "${CPP_FILES[@]}"
fi
