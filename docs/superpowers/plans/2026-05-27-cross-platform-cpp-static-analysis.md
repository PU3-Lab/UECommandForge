# Cross-Platform C++ Static Analysis Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a repo-local C++ static analysis entry point that works on macOS, Linux, and Windows Git Bash without bundling LLVM or Cppcheck into the Unreal plugin package.

**Architecture:** Add focused wrappers under `tools/lint/`: a Bash wrapper for macOS/Linux/Windows Git Bash, a PowerShell wrapper for Windows native use, and a batch wrapper for Windows Command Prompt. Each wrapper discovers `clang-tidy` and `cppcheck` from environment variables, PATH, Homebrew LLVM, Windows LLVM, Cppcheck install paths, and Visual Studio LLVM candidates. Keep Unreal plugin distribution clean by documenting external prerequisites instead of vendoring analyzer binaries.

**Tech Stack:** Bash, PowerShell, Windows batch, Homebrew LLVM, LLVM for Windows, Cppcheck, Unreal plugin C++ source files.

## Current Implementation Status

This plan is historical; the repo implementation now includes the follow-up CI hardening work.

- `tools/lint/cpp_static_analysis.sh`, `.ps1`, and `.bat` are the supported entry points.
- Default local and push/PR CI analysis runs `cppcheck` only.
- `clang-tidy` is opt-in through `--clang-tidy-only` or `RUN_CLANG_TIDY=1` after generating `compile_commands.json`.
- Windows native CI validates PowerShell and Command Prompt wrappers, including the no-compile-database `clang-tidy` skip path.
- `C++ Static Analysis` uses `actions/checkout@v6` and pins the Windows job to `windows-2025-vs2026`.
- `workflow_dispatch` exposes `run_clang_tidy`; when true, the macOS job generates `compile_commands.json` and runs opt-in `clang-tidy`. This requires Unreal Engine on the runner; use the `ue_root` input if the engine is not in the default path.
- Windows `clang-tidy` discovery checks `CLANG_TIDY`, PATH, LLVM's default install path, Visual Studio 2026 LLVM paths, then Visual Studio 2022 LLVM paths.

---

### Task 1: Tool Detection Test

**Files:**
- Create: `tools/test/lint/cpp_static_analysis_detection.sh`
- Create: `tools/lint/cpp_static_analysis.sh`

- [ ] **Step 1: Write the failing test**

Create `tools/test/lint/cpp_static_analysis_detection.sh`:

```bash
#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
TMP_DIR="$(mktemp -d)"
trap 'rm -rf "${TMP_DIR}"' EXIT

FAKE_BIN="${TMP_DIR}/bin"
mkdir -p "${FAKE_BIN}"

cat > "${FAKE_BIN}/clang-tidy.exe" <<'FAKE'
#!/usr/bin/env bash
echo "fake clang-tidy"
FAKE
chmod +x "${FAKE_BIN}/clang-tidy.exe"

cat > "${FAKE_BIN}/cppcheck.exe" <<'FAKE'
#!/usr/bin/env bash
echo "fake cppcheck"
FAKE
chmod +x "${FAKE_BIN}/cppcheck.exe"

OUTPUT="$(
  STATIC_ANALYSIS_PLATFORM=windows \
  PATH="${FAKE_BIN}:${PATH}" \
  "${REPO_ROOT}/tools/lint/cpp_static_analysis.sh" --check-tools
)"

echo "${OUTPUT}" | grep -q "clang-tidy: ${FAKE_BIN}/clang-tidy.exe"
echo "${OUTPUT}" | grep -q "cppcheck: ${FAKE_BIN}/cppcheck.exe"
echo "${OUTPUT}" | grep -q "platform: windows"
```

- [ ] **Step 2: Run test to verify it fails**

Run: `bash tools/test/lint/cpp_static_analysis_detection.sh`

Expected: FAIL because `tools/lint/cpp_static_analysis.sh` does not exist yet.

### Task 2: Static Analysis Wrapper

**Files:**
- Modify: `tools/lint/cpp_static_analysis.sh`
- Modify: `tools/test/lint/cpp_static_analysis_detection.sh`

- [ ] **Step 1: Write minimal implementation**

Create `tools/lint/cpp_static_analysis.sh` with:

```bash
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
      "/c/Program Files/Microsoft Visual Studio/2026/Community/VC/Tools/Llvm/x64/bin/clang-tidy.exe" \
      "/c/Program Files/Microsoft Visual Studio/2026/Professional/VC/Tools/Llvm/x64/bin/clang-tidy.exe" \
      "/c/Program Files/Microsoft Visual Studio/2026/Enterprise/VC/Tools/Llvm/x64/bin/clang-tidy.exe" \
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
      "${path_candidate}" \
      "$(path_command cppcheck.exe)" \
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

mapfile -t CPP_FILES < <(find "${SOURCE_ROOT}" -type f \( -name '*.cpp' -o -name '*.h' \) | sort)

if [ "${MODE}" != "clang-tidy-only" ]; then
  if [ -z "${CPPCHECK_BIN}" ]; then
    echo "[lint] cppcheck not found. Install it or set CPPCHECK." >&2
    exit 127
  fi
  "${CPPCHECK_BIN}" --enable=warning,style,performance,portability --inline-suppr --std=c++20 --suppress=missingIncludeSystem "${CPP_FILES[@]}"
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
```

- [ ] **Step 2: Run test to verify it passes**

Run: `bash tools/test/lint/cpp_static_analysis_detection.sh`

Expected: PASS.

### Task 3: Windows PowerShell Wrapper

**Files:**
- Create: `tools/test/lint/cpp_static_analysis_windows_wrapper.sh`
- Create: `tools/lint/cpp_static_analysis.ps1`

- [ ] **Step 1: Write the failing wrapper contract test**

Create `tools/test/lint/cpp_static_analysis_windows_wrapper.sh` to assert that the PowerShell entry point exists, exposes `-CheckTools`, `-CppcheckOnly`, and `-ClangTidyOnly`, and includes Windows LLVM/Cppcheck install path candidates.

- [ ] **Step 2: Run test to verify it fails**

Run: `bash tools/test/lint/cpp_static_analysis_windows_wrapper.sh`

Expected: FAIL because `tools/lint/cpp_static_analysis.ps1` does not exist yet.

- [ ] **Step 3: Add the PowerShell wrapper**

Create `tools/lint/cpp_static_analysis.ps1` with PowerShell-native tool discovery and the same behavior as the Bash wrapper.

- [ ] **Step 4: Run test to verify it passes**

Run: `bash tools/test/lint/cpp_static_analysis_windows_wrapper.sh`

Expected: PASS.

### Task 4: Windows Command Prompt Wrapper

**Files:**
- Create: `tools/test/lint/cpp_static_analysis_bat_wrapper.sh`
- Create: `tools/lint/cpp_static_analysis.bat`
- Modify: `README.md`

- [ ] **Step 1: Write the failing batch wrapper contract test**

Create `tools/test/lint/cpp_static_analysis_bat_wrapper.sh` to assert that the batch entry point exists, exposes `--check-tools`, `--cppcheck-only`, and `--clang-tidy-only`, and includes Windows LLVM/Cppcheck install path candidates.

- [ ] **Step 2: Run test to verify it fails**

Run: `bash tools/test/lint/cpp_static_analysis_bat_wrapper.sh`

Expected: FAIL because `tools/lint/cpp_static_analysis.bat` does not exist yet.

- [ ] **Step 3: Add the batch wrapper**

Create `tools/lint/cpp_static_analysis.bat` with cmd.exe-native tool discovery and behavior matching the Bash and PowerShell wrappers.

- [ ] **Step 4: Run test to verify it passes**

Run: `bash tools/test/lint/cpp_static_analysis_bat_wrapper.sh`

Expected: PASS.

### Task 5: Documentation And Verification

**Files:**
- Modify: `README.md`

- [ ] **Step 1: Document macOS and Windows prerequisites**

Add README guidance that static analysis tools are development prerequisites, not part of the Unreal plugin package. Document the three supported entry points:

```text
macOS/Linux/Windows Git Bash: tools/lint/cpp_static_analysis.sh
Windows PowerShell:          tools/lint/cpp_static_analysis.ps1
Windows Command Prompt:      tools/lint/cpp_static_analysis.bat
```

- [ ] **Step 2: Verify on current host**

Run:

```bash
tools/lint/cpp_static_analysis.sh --check-tools
bash tools/test/lint/cpp_static_analysis_detection.sh
bash tools/test/lint/cpp_static_analysis_windows_wrapper.sh
bash tools/test/lint/cpp_static_analysis_bat_wrapper.sh
tools/lint/cpp_static_analysis.sh
git diff --check
```

Expected:
- `--check-tools` reports installed `clang-tidy` and `cppcheck`.
- Detection tests pass.
- The full Bash wrapper runs `cppcheck`; if `compile_commands.json` is missing, it skips `clang-tidy` with an explanatory message.
- `git diff --check` prints no errors.

- [ ] **Step 3: Commit**

```bash
git add README.md docs/superpowers/plans/2026-05-27-cross-platform-cpp-static-analysis.md tools/lint/cpp_static_analysis.sh tools/lint/cpp_static_analysis.ps1 tools/lint/cpp_static_analysis.bat tools/test/lint/cpp_static_analysis_detection.sh tools/test/lint/cpp_static_analysis_windows_wrapper.sh tools/test/lint/cpp_static_analysis_bat_wrapper.sh
git commit -m "chore: add cross-platform cpp static analysis wrapper"
```
