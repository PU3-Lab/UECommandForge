# UECommandForge 범용 AI 코딩 도구 지원 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [x]`) syntax for tracking.

**Goal:** Codex 전용 결합을 제거하고 `--codex`/`--claude`/`--antigravity` 플래그로 각 에이전트 전역 폴더에 설치되도록 UECommandForge를 범용화한다.

**Architecture:** 네 개 층(런타임 출력 경로, 설치 프로파일, 지시/스펙 콘텐츠, manifest/언인스톨러)을 단계적으로 중립화한다. 흩어진 `Saved/CodexReports` 리터럴은 단일 상수로 추출하고, 설치 스크립트는 에이전트 프로파일 테이블 기반으로 재구성한다. Windows는 별도 로직 없이 Git Bash로 동일 bash 코어를 실행한다.

**Tech Stack:** Unreal C++ (Editor commandlets), Bash 설치/래퍼 스크립트, `.bat`/`.ps1` 얇은 래퍼, jq, shell smoke 테스트.

**Spec:** [docs/superpowers/specs/2026-06-11-multi-agent-support-design.md](../specs/2026-06-11-multi-agent-support-design.md)

---

## File Structure

### Phase A — 런타임 출력 경로 (`Saved/CodexReports` → `Saved/UECommandForge/Reports`)
- Create: `sample/Plugins/UECommandForge/Source/UECommandForgeEditor/Public/Reports/ReportPaths.h` — 리포트 디렉터리 단일 상수/헬퍼
- Modify: 4개 commandlet `.cpp` (rollback 디렉터리 + 가드 메시지)
- Modify: `tools/ue/run_commandlet.sh`, `tools/ue/run_commandlet.bat` — `REPORT_DIR` 기본값/주석
- Modify: C++ 테스트 21개, shell smoke 24개, `tools/test/smoke/create_ai_flow.bat`, `.gitignore`
- Modify: 문서(`AGENTS.md`, `README.md`, 계획/워크스루 문서)

### Phase B — 지시/스펙 콘텐츠 중립화
- Move: `specs/codex/unreal-automation-agents.md` → `specs/agent/unreal-automation-agents.md`
- Modify: 위 파일 본문("Codex가" → "AI 코딩 에이전트가", 경로 일반화)
- Modify: `AGENTS.md`(루트) 참조, `skills/uecommandforge/SKILL.md`

### Phase C — 설치 프로파일 (멀티 에이전트)
- Modify: `tools/release/install_local.sh` — 프로파일 테이블, 에이전트 플래그, 내부 변수 리네임, 마커 마이그레이션
- Modify: `install-uecommandforge.sh` — 플래그 passthrough + 도움말, `install-uecommandforge.bat`/`.ps1` 도움말
- Modify: `tools/release/uninstall.sh`, manifest 키
- Modify: 설치 smoke 테스트(`HOME` 격리 + per-agent/multi-agent)

---

## Phase A — 런타임 출력 경로 중립화

`Saved/CodexReports` 를 그룹화된 `Saved/UECommandForge/Reports` 로 바꾸고, 흩어진 C++ 리터럴을 단일 상수로 추출한다.

### Task A1: C++ 리포트 경로 단일 상수 추출

**Files:**
- Create: `sample/Plugins/UECommandForge/Source/UECommandForgeEditor/Public/Reports/ReportPaths.h`
- Modify: `sample/Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Commandlets/ApplyAssetChangesCommandlet.cpp:217-264`
- Modify: `sample/Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Commandlets/PlanAssetChangesCommandlet.cpp:217-264`
- Modify: `sample/Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Commandlets/ImportDataSourceCommandlet.cpp:166-221`
- Modify: `sample/Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Commandlets/CreateProjectFoldersCommandlet.cpp:86-89`

- [x] **Step 1: 공유 헤더 작성**

`ReportPaths.h` 생성:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Misc/Paths.h"

namespace UECommandForge
{
    /** 프로젝트 Saved 아래 UECommandForge 리포트 서브경로. 모든 commandlet이 이 상수만 참조한다. */
    inline const TCHAR* ReportsSubDir()
    {
        return TEXT("UECommandForge/Reports");
    }

    /** 정규화된 절대 리포트 디렉터리. (= <Project>/Saved/UECommandForge/Reports) */
    inline FString GetReportsDir()
    {
        FString ReportsDir = FPaths::ConvertRelativePathToFull(
            FPaths::Combine(FPaths::ProjectSavedDir(), ReportsSubDir()));
        FPaths::NormalizeDirectoryName(ReportsDir);
        FPaths::CollapseRelativeDirectories(ReportsDir);
        return ReportsDir;
    }
}
```

- [x] **Step 2: ApplyAssetChangesCommandlet.cpp 갱신**

상단 include에 `#include "Reports/ReportPaths.h"` 추가. `RollbackReportDirectory()` 본문을 교체:

```cpp
    FString RollbackReportDirectory()
    {
        return UECommandForge::GetReportsDir();
    }
```

가드 메시지(라인 262 부근) 교체:

```cpp
            AddError(Report, TEXT("ROLLBACK_PATH_OUTSIDE_REPORT_DIR"),
                TEXT("rollback_plan은 Saved/UECommandForge/Reports 아래에만 쓸 수 있습니다."),
                TEXT("rollback_plan"));
```

- [x] **Step 3: PlanAssetChangesCommandlet.cpp 갱신**

`#include "Reports/ReportPaths.h"` 추가 후 동일하게 `RollbackReportDirectory()` 본문을 `return UECommandForge::GetReportsDir();` 로, 가드 메시지를 `TEXT("rollback_plan은 Saved/UECommandForge/Reports 아래에만 쓸 수 있습니다.")` 로 교체.

- [x] **Step 4: ImportDataSourceCommandlet.cpp 갱신**

`#include "Reports/ReportPaths.h"` 추가. 라인 169 부근 `FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CodexReports"))` 를 `UECommandForge::GetReportsDir()` 호출로 교체하고, 라인 219 부근 가드 메시지를 `TEXT("rollback_plan은 Saved/UECommandForge/Reports 아래에만 쓸 수 있습니다.")` 로 교체.

- [x] **Step 5: CreateProjectFoldersCommandlet.cpp 갱신**

`#include "Reports/ReportPaths.h"` 추가 후 `RollbackReportDirectory()` 본문 교체:

```cpp
    FString RollbackReportDirectory()
    {
        return UECommandForge::GetReportsDir();
    }
```

- [x] **Step 6: 생산 코드에 옛 리터럴이 남지 않았는지 확인**

Run:
```bash
grep -rn 'CodexReports' sample/Plugins/UECommandForge/Source/UECommandForgeEditor/Private/
```
Expected: 출력 없음 (빈 결과).

- [x] **Step 7: 커밋**

```bash
git add sample/Plugins/UECommandForge/Source/UECommandForgeEditor/Public/Reports/ReportPaths.h \
  sample/Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Commandlets/ApplyAssetChangesCommandlet.cpp \
  sample/Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Commandlets/PlanAssetChangesCommandlet.cpp \
  sample/Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Commandlets/ImportDataSourceCommandlet.cpp \
  sample/Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Commandlets/CreateProjectFoldersCommandlet.cpp
git commit -m "refactor: extract report dir constant and rename to Saved/UECommandForge/Reports"
```

### Task A2: 래퍼 계층 REPORT_DIR 기본값 변경

**Files:**
- Modify: `tools/ue/run_commandlet.sh:3,81`
- Modify: `tools/ue/run_commandlet.bat:33` (+ 상단 주석)

- [x] **Step 1: run_commandlet.sh 갱신**

라인 81 교체:
```bash
REPORT_DIR="${UECF_REPORT_DIR:-${PROJECT_DIR}/Saved/UECommandForge/Reports}"
```
라인 3 주석 교체:
```bash
# Result JSON은 대상 project의 Saved/UECommandForge/Reports/<Commandlet>_<UTC>.json에 기록된다.
```

- [x] **Step 2: run_commandlet.bat 갱신**

라인 33 교체:
```bat
  set "REPORT_DIR=!PROJECT_DIR!\Saved\UECommandForge\Reports"
```
상단의 `Saved\CodexReports` 주석도 `Saved\UECommandForge\Reports` 로 교체.

- [x] **Step 3: 래퍼에 옛 경로가 남지 않았는지 확인**

Run: `grep -rn 'CodexReports' tools/ue/`
Expected: 출력 없음.

- [x] **Step 4: 커밋**

```bash
git add tools/ue/run_commandlet.sh tools/ue/run_commandlet.bat
git commit -m "refactor: point commandlet wrapper report dir to Saved/UECommandForge/Reports"
```

### Task A3: C++ 테스트 경로 일괄 변경

**Files:** `sample/Plugins/UECommandForge/Source/UECommandForgeEditor/Tests/` 아래 `CodexReports` 를 참조하는 21개 `.cpp`.

- [x] **Step 1: 변경 전 참조 개수 기록**

Run: `grep -rl 'CodexReports' sample/Plugins/UECommandForge/Source/UECommandForgeEditor/Tests/ | wc -l`
Expected: `21`

- [x] **Step 2: 테스트의 단일 세그먼트 리터럴을 두 세그먼트로 치환**

테스트는 `TEXT("CodexReports")` 를 단일 디렉터리 세그먼트로 `FPaths::Combine(..., TEXT("CodexReports"), TEXT("foo.json"))` 형태로 쓴다. 이를 `TEXT("UECommandForge"), TEXT("Reports")` 두 세그먼트로 바꾼다.

Run (검토용 dry-run 먼저):
```bash
grep -rn 'TEXT("CodexReports")' sample/Plugins/UECommandForge/Source/UECommandForgeEditor/Tests/ | head
```
그 다음 일괄 치환(macOS):
```bash
grep -rl 'TEXT("CodexReports")' sample/Plugins/UECommandForge/Source/UECommandForgeEditor/Tests/ \
  | xargs sed -i '' 's/TEXT("CodexReports")/TEXT("UECommandForge"), TEXT("Reports")/g'
```
> macOS `sed` 는 `-i ''` 를 쓴다. Linux/Git Bash는 `sed -i`.

- [x] **Step 3: 남은 옛 리터럴 확인**

Run: `grep -rn 'CodexReports' sample/Plugins/UECommandForge/Source/UECommandForgeEditor/Tests/`
Expected: 출력 없음. (혹시 `FPaths::Combine` 인자가 아닌 문자열/주석에 남아 있으면 수동으로 `UECommandForge/Reports` 로 교체.)

- [x] **Step 4: 빌드 + C++ 자동화 테스트 실행 (UE 필요)**

Run (예시, 실제 엔진 경로/프로젝트로 대체):
```bash
UECF_PROJECT_FILE=<.uproject> tools/test/smoke/asset_change_apply_rollback.sh
```
Expected: rollback 산출물이 `Saved/UECommandForge/Reports` 아래에 생성되고 가드 테스트 통과.

- [x] **Step 5: 커밋**

```bash
git add sample/Plugins/UECommandForge/Source/UECommandForgeEditor/Tests/
git commit -m "test: update C++ commandlet tests to Saved/UECommandForge/Reports"
```

### Task A4: shell/bat smoke 테스트 + .gitignore 경로 변경

**Files:** `tools/test/smoke/*.sh`(24개 중 `CodexReports` 참조분), `tools/test/smoke/create_ai_flow.bat`, `.gitignore:17`

- [x] **Step 1: 변경 전 참조 파일 목록 확인**

Run: `grep -rl 'CodexReports' tools/test/smoke/`
Expected: `.sh` 다수 + `create_ai_flow.bat`.

- [x] **Step 2: shell smoke 일괄 치환**

```bash
grep -rl 'Saved/CodexReports' tools/test/smoke/ \
  | xargs sed -i '' 's#Saved/CodexReports#Saved/UECommandForge/Reports#g'
grep -rl 'CodexReports' tools/test/smoke/ \
  | grep '\.sh$' | xargs --no-run-if-empty sed -i '' 's#CodexReports#UECommandForge/Reports#g'
```

- [x] **Step 3: bat smoke 치환**

`tools/test/smoke/create_ai_flow.bat` 의 `Saved\CodexReports` → `Saved\UECommandForge\Reports`, 단독 `CodexReports` → `UECommandForge\Reports`.

- [x] **Step 4: .gitignore 주석 갱신**

라인 17 `# But keep CodexReports examples` → `# But keep UECommandForge/Reports examples`.

- [x] **Step 5: 남은 참조 확인**

Run: `grep -rn 'CodexReports' tools/ .gitignore`
Expected: 출력 없음.

- [x] **Step 6: 대표 smoke 1개 실행**

Run: `UECF_PROJECT_FILE=<.uproject> tools/test/smoke/create_ai_flow.sh`
Expected: 결과 JSON이 `Saved/UECommandForge/Reports` 에 생성되고 PASS.

- [x] **Step 7: 커밋**

```bash
git add tools/test/smoke/ .gitignore
git commit -m "test: update smoke tests and gitignore to Saved/UECommandForge/Reports"
```

### Task A5: 문서 경로 갱신

**Files:** `AGENTS.md`, `README.md`, `ue_commandlet_based_llm_automation_plan.md`, `docs/**`(CodexReports 참조분)

- [x] **Step 1: 참조 목록 확인**

Run: `grep -rl 'CodexReports' AGENTS.md README.md ue_commandlet_based_llm_automation_plan.md docs/`

- [x] **Step 2: 문서 일괄 치환**

```bash
grep -rl 'Saved/CodexReports' AGENTS.md README.md ue_commandlet_based_llm_automation_plan.md docs/ \
  | xargs sed -i '' 's#Saved/CodexReports#Saved/UECommandForge/Reports#g'
grep -rl 'CodexReports' AGENTS.md README.md ue_commandlet_based_llm_automation_plan.md docs/ \
  | xargs --no-run-if-empty sed -i '' 's#CodexReports#UECommandForge/Reports#g'
```
> `docs/superpowers/plans/`, `docs/memory/` 의 과거 세션 기록은 역사적 사실이므로 **경로 문자열만** 바뀌어도 무방하다. 의미가 깨지는 문장이 있으면 수동 검토.

- [x] **Step 3: 남은 참조 확인**

Run: `grep -rn 'CodexReports' . --include='*.md' | grep -v node_modules`
Expected: 출력 없음.

- [x] **Step 4: 커밋**

```bash
git add -A
git commit -m "docs: update report path references to Saved/UECommandForge/Reports"
```

---

## Phase B — 지시/스펙 콘텐츠 중립화

### Task B1: 규칙 스펙 이동 + 중립화

**Files:**
- Move: `specs/codex/unreal-automation-agents.md` → `specs/agent/unreal-automation-agents.md`
- Modify: 이동한 파일 본문

- [x] **Step 1: git mv 로 이동**

```bash
mkdir -p specs/agent
git mv specs/codex/unreal-automation-agents.md specs/agent/unreal-automation-agents.md
rmdir specs/codex 2>/dev/null || true
```

- [x] **Step 2: 제목/도입부 중립화**

`specs/agent/unreal-automation-agents.md` 1~3행 교체:
```markdown
# UECommandForge Unreal 자동화 작업 규칙

이 문서는 AI 코딩 에이전트가 UECommandForge 프로젝트에서 Unreal Editor, 에셋, Blueprint, C++ Reflection, DataTable, Config 관련 자동화를 수행할 때 반드시 따라야 하는 규칙이다.
```

- [x] **Step 3: 본문의 "Codex" 주체 표현 중립화**

`Codex가` → `에이전트가`, `Codex는` → `에이전트는`, `Codex skill` → `uecommandforge skill`, `Codex 최종 판단` → `에이전트 최종 판단` 으로 치환:
```bash
sed -i '' \
  -e 's/Codex가/에이전트가/g' \
  -e 's/Codex는/에이전트는/g' \
  -e 's/Codex skill/uecommandforge skill/g' \
  -e 's/Codex 최종 판단/에이전트 최종 판단/g' \
  specs/agent/unreal-automation-agents.md
```

- [x] **Step 4: 하드코딩 홈 경로 일반화**

`~/.codex/UECommandForge/...` 를 `<AGENT_HOME>/UECommandForge/...` 로 교체:
```bash
sed -i '' 's#~/.codex/UECommandForge#<AGENT_HOME>/UECommandForge#g' specs/agent/unreal-automation-agents.md
```
설치 확인 절차(4절)의 `~/.codex/UECommandForge/uecommandforge-installed.json` 등도 `<AGENT_HOME>/UECommandForge/...` 로 바뀐다. `Saved/CodexReports` 잔여분이 있으면 `Saved/UECommandForge/Reports` 로 교체.

- [x] **Step 5: MANAGED CONTENT 블록 내부도 동일하게 중립화됐는지 확인**

Run: `sed -n '/BEGIN UECOMMANDFORGE MANAGED CONTENT/,/END UECOMMANDFORGE MANAGED CONTENT/p' specs/agent/unreal-automation-agents.md`
Expected: 블록 안에 `Codex`/`CodexReports`/`~/.codex` 가 없다. 남았으면 수동 교체.

- [x] **Step 6: 잔여 확인 + 커밋**

```bash
grep -n 'Codex\|~/.codex\|CodexReports' specs/agent/unreal-automation-agents.md   # 기대: 출력 없음
git add -A
git commit -m "refactor: move agent rules to specs/agent and neutralize Codex wording"
```

### Task B2: 루트 AGENTS.md + SKILL.md 참조 갱신

**Files:**
- Modify: `AGENTS.md:26,28`
- Modify: `skills/uecommandforge/SKILL.md`

- [x] **Step 1: 루트 AGENTS.md 갱신**

라인 26 교체:
```markdown
- 안전 규칙은 `specs/agent/unreal-automation-agents.md`를 따른다.
```
라인 28의 `최신 Saved/CodexReports/*.json` → `최신 Saved/UECommandForge/Reports/*.json` (Phase A5에서 이미 바뀌었으면 확인만).

- [x] **Step 2: SKILL.md 중립화**

`skills/uecommandforge/SKILL.md` 에서 `Codex` 고유 표현을 중립화:
```bash
grep -n 'Codex\|~/.codex\|CodexReports\|specs/codex' skills/uecommandforge/SKILL.md
```
- `Codex skill` → `uecommandforge skill`, `Codex가/Codex는` → `에이전트가/에이전트는`
- `~/.codex/...` → `<AGENT_HOME>/...`, `specs/codex/` → `specs/agent/`, `CodexReports` → `UECommandForge/Reports`
각 매치를 위 규칙대로 교체한다.

- [x] **Step 3: 잔여 확인**

Run: `grep -rn 'specs/codex\|CodexReports' AGENTS.md skills/uecommandforge/SKILL.md`
Expected: 출력 없음.

- [x] **Step 4: 커밋**

```bash
git add AGENTS.md skills/uecommandforge/SKILL.md
git commit -m "docs: point AGENTS.md and SKILL.md to neutralized specs/agent paths"
```

---

## Phase C — 설치 프로파일 (멀티 에이전트)

가장 로직 변화가 큰 단계. 에이전트 프로파일 테이블, 다중 플래그, 마커 마이그레이션, manifest 키 중립화를 다룬다.

### Task C1: install_local.sh 에이전트 프로파일 + 다중 플래그

**Files:** `tools/release/install_local.sh`

이 스크립트는 단일 `CODEX_HOME` 가정으로 짜여 있다. 핵심 설치 본문(스테이징·검증·복사·manifest 작성)을 **에이전트 1개를 받는 함수** `install_for_agent <agent>` 로 감싸고, 인자로 받은 에이전트 목록을 순회한다.

- [x] **Step 1: 인자 파서 교체**

상단 인자 파싱(라인 7~45 영역)에서 `--codex-home` 케이스를 제거하고 에이전트 플래그를 추가한다. `AGENTS=()` 배열에 수집:

```bash
PROJECT_FILE="${UECF_PROJECT:-${PROJECT_FILE:-}}"
PLUGIN_PACKAGE=""
TOOLS_PACKAGE=""
AGENTS=()
BACKUP=true
RUN_COMMANDLET_CHECK=false

while [ $# -gt 0 ]; do
  case "$1" in
    --project) PROJECT_FILE="$2"; shift 2 ;;
    --plugin-package|--local-package) PLUGIN_PACKAGE="$2"; shift 2 ;;
    --tools-package) TOOLS_PACKAGE="$2"; shift 2 ;;
    --codex) AGENTS+=("codex"); shift ;;
    --claude) AGENTS+=("claude"); shift ;;
    --antigravity) AGENTS+=("antigravity"); shift ;;
    --backup) BACKUP="$2"; shift 2 ;;
    --run-commandlet-check) RUN_COMMANDLET_CHECK="$2"; shift 2 ;;
    *) echo "[install_local] Unknown argument: $1" >&2; exit 2 ;;
  esac
done
```

- [x] **Step 2: 에이전트 필수 검증 + 중복 제거**

인자 검증 블록(라인 47~53 영역) 다음에 추가:

```bash
if [ "${#AGENTS[@]}" -eq 0 ]; then
  echo "[install_local] 최소 하나의 에이전트 플래그가 필요합니다: --codex | --claude | --antigravity" >&2
  exit 2
fi
# 중복 플래그 제거(순서 유지)
DEDUP_AGENTS=()
for agent in "${AGENTS[@]}"; do
  case " ${DEDUP_AGENTS[*]} " in *" ${agent} "*) ;; *) DEDUP_AGENTS+=("${agent}") ;; esac
done
AGENTS=("${DEDUP_AGENTS[@]}")
```

- [x] **Step 3: 프로파일 해석 함수 추가**

PROJECT_FILE 정규화 직후에 추가:

```bash
agent_home_for() {
  case "$1" in
    codex) printf '%s/.codex' "${HOME}" ;;
    claude) printf '%s/.claude' "${HOME}" ;;
    antigravity) printf '%s/.gemini' "${HOME}" ;;
    *) echo "[install_local] unknown agent: $1" >&2; exit 2 ;;
  esac
}

agent_instructions_filename_for() {
  case "$1" in
    codex) printf 'AGENTS.md' ;;
    claude) printf 'CLAUDE.md' ;;
    antigravity) printf 'GEMINI.md' ;;
    *) echo "[install_local] unknown agent: $1" >&2; exit 2 ;;
  esac
}
```

- [x] **Step 4: 설치 본문을 install_for_agent 함수로 감싸기**

기존 `CODEX_HOME=...` 이후의 경로 변수 정의부터 manifest/로그 작성까지(라인 73~546 영역)를 함수 본문으로 옮기고, 첫머리에서 에이전트별 경로를 계산한다. 변수 리네임 매핑:

```
CODEX_HOME            → AGENT_HOME
CODEX_SKILL_DIR       → SKILL_DIR
CODEX_AGENTS_FILE     → INSTRUCTIONS_FILE
CODEX_ENV_FILE        → AGENT_ENV_FILE
CODEX_SHELL_ENV_FILE  → AGENT_SHELL_ENV_FILE
```

함수 머리:

```bash
install_for_agent() {
  local AGENT="$1"
  local AGENT_HOME SKILL_DIR INSTRUCTIONS_FILE
  AGENT_HOME="$(agent_home_for "${AGENT}")"
  uecf_reject_link_ancestors "${AGENT_HOME}" "install_local"
  AGENT_HOME="$(mkdir -p "${AGENT_HOME}" && cd "${AGENT_HOME}" && pwd -P)"
  local INSTALL_ROOT="${AGENT_HOME}/UECommandForge"
  SKILL_DIR="${AGENT_HOME}/skills/uecommandforge"
  INSTRUCTIONS_FILE="${AGENT_HOME}/$(agent_instructions_filename_for "${AGENT}")"
  local AGENT_ENV_FILE="${INSTALL_ROOT}/uecommandforge.env"
  local AGENT_SHELL_ENV_FILE="${INSTALL_ROOT}/uecommandforge.env.sh"
  local INSTALLED_MANIFEST="${INSTALL_ROOT}/uecommandforge-installed.json"
  # ... (기존 본문: 백업, 복사, append, manifest 작성) ...
}
```
스테이징/패키지 검증(staged package 압축 해제, 버전 확인)은 에이전트와 무관하므로 함수 **밖에서 1회** 수행하고, 함수는 `${PLUGIN_ROOT}`/`${TOOLS_ROOT}`/`${VERSION}` 등 공통 변수를 참조한다. 플러그인 복사 대상(`PLUGIN_DIR`)·프로젝트 manifest·로그는 에이전트와 무관(프로젝트 쪽)하므로 첫 에이전트에서 1회만 수행하거나 함수 밖에서 처리한다.

- [x] **Step 5: 마커를 중립화하고 옛 마커 마이그레이션**

`append_codex_agents_instructions`(라인 297~343) 를 `append_agent_instructions` 로 바꾸고 마커를 `UECOMMANDFORGE INSTRUCTIONS` 로 변경. 기존 블록 제거 awk가 **옛/새 마커 둘 다** 인식하게 한다:

```bash
cat >> "${temp_file}" <<'INSTR'
<!-- BEGIN UECOMMANDFORGE INSTRUCTIONS -->
INSTR
```
제거 awk 패턴:
```bash
awk '
  /BEGIN UECOMMANDFORGE (CODEX )?INSTRUCTIONS/ { skipping = 1; next }
  /END UECOMMANDFORGE (CODEX )?INSTRUCTIONS/ { skipping = 0; next }
  skipping != 1 { print }
' "${INSTRUCTIONS_FILE}" > "${temp_file}.stripped"
```
`require_ordered_marker_pair` 호출의 마커 인자도 새 마커로 바꾸되, begin_count 계산은 옛/새 둘 다 합산하도록 `grep -cE 'BEGIN UECOMMANDFORGE (CODEX )?INSTRUCTIONS'` 로 변경. 지시 소스 발췌는 `${TOOLS_ROOT}/specs/agent/unreal-automation-agents.md` 에서 한다. `require_codex_agents_appendable`/`require_instruction_source` 함수명·메시지의 "Codex" 도 중립화한다.

- [x] **Step 6: manifest/프로젝트 manifest 키 중립화**

installed manifest(라인 467~) 와 project manifest(라인 508~) 의 jq 키를 교체: `codex_home`→`agent_home`, `codex_tools_path`→`tools_path`, `codex_specs_path`→`specs_path`, `codex_skill_path`→`skill_path`. installed manifest에 `agent: $agent`(에이전트 이름) 필드를 추가. env 파일 내용의 `CODEX_HOME=` → `AGENT_HOME=`.
> 프로젝트 manifest는 에이전트 무관 공통 정보(project_file, plugin_path, installed_version)만 기록하고, 에이전트별 홈 경로는 각 홈의 installed manifest(홈마다 1개)에 둔다. 다중 에이전트 설치 시 프로젝트 manifest는 동일 내용으로 1회만 쓴다.

- [x] **Step 7: 에이전트 순회 호출**

스테이징/검증 이후 마지막에:
```bash
for agent in "${AGENTS[@]}"; do
  install_for_agent "${agent}"
done
echo "installed agents: ${AGENTS[*]}"
```

- [x] **Step 8: 잔여 Codex 식별자 확인**

Run: `grep -n 'CODEX\|Codex\|codex' tools/release/install_local.sh`
Expected: `specs/agent` 경로·중립 메시지만 남고 `CODEX_HOME`/`--codex-home`/`codex_home` 식별자는 없음.

- [x] **Step 9: 구문 검사 + 커밋**

```bash
bash -n tools/release/install_local.sh
git add tools/release/install_local.sh
git commit -m "feat: install_local supports --codex/--claude/--antigravity profiles"
```

### Task C2: 상위 설치 스크립트 + Windows 래퍼 도움말

**Files:** `install-uecommandforge.sh`, `install-uecommandforge.bat`, `install-uecommandforge.ps1`

- [x] **Step 1: passthrough 파서에 에이전트 플래그 추가**

`install-uecommandforge.sh` 의 passthrough case(라인 78 `--codex-home|--backup|--run-commandlet-check`)를 교체:
```bash
    --backup|--run-commandlet-check)
      INSTALL_ARGS+=("$1" "$2"); shift 2 ;;
    --codex|--claude|--antigravity)
      INSTALL_ARGS+=("$1"); shift ;;
```
`--codex-home` 케이스를 제거한다.

- [x] **Step 2: 도움말 텍스트 갱신**

라인 32 `--codex-home <path> ... Defaults to ~/.codex` 블록을 제거하고 다음으로 교체:
```
  --codex | --claude | --antigravity
      설치 대상 AI 코딩 에이전트(전역 폴더). 하나 이상 지정한다.
      --codex → ~/.codex/AGENTS.md, --claude → ~/.claude/CLAUDE.md,
      --antigravity → ~/.gemini/GEMINI.md
```
사용 예시(라인 15~21)에도 `--codex` 를 추가한다. 예: `./install-uecommandforge.sh --project <.uproject> --codex`.

- [x] **Step 3: .bat/.ps1 사용 예시 갱신**

`install-uecommandforge.bat`, `install-uecommandforge.ps1` 의 주석/도움말 예시에 `--codex` 등 에이전트 플래그를 추가한다(로직은 passthrough라 변경 없음).

- [x] **Step 4: 구문 검사 + 커밋**

```bash
bash -n install-uecommandforge.sh
git add install-uecommandforge.sh install-uecommandforge.bat install-uecommandforge.ps1
git commit -m "feat: forward agent flags through top-level installer and update help"
```

### Task C3: uninstall.sh 중립화 + 옛 키 fallback

**Files:** `tools/release/uninstall.sh`

- [x] **Step 1: 플래그/변수 교체**

`--codex-home` 를 에이전트 플래그(`--codex|--claude|--antigravity`)로 교체하고, C1의 `agent_home_for` 와 동일한 case 함수를 추가해 에이전트→홈을 해석한다. `CODEX_HOME` → `AGENT_HOME`. `--remove-codex-tools` → `--remove-agent-tools`(옛 플래그는 별칭으로 계속 인식). 에이전트 플래그가 최소 하나 필수이며, 여러 개면 각 홈에서 순회 제거한다.

- [x] **Step 2: installed manifest 키 읽기에 fallback**

manifest에서 경로를 읽을 때 신규 키 우선, 옛 키 fallback:
```bash
TOOLS_PATH="$(jq -r '.tools_path // .codex_tools_path' "${INSTALLED_MANIFEST}")"
SPECS_PATH="$(jq -r '.specs_path // .codex_specs_path' "${INSTALLED_MANIFEST}")"
SKILL_PATH="$(jq -r '.skill_path // .codex_skill_path' "${INSTALLED_MANIFEST}")"
AGENT_HOME="$(jq -r '.agent_home // .codex_home' "${INSTALLED_MANIFEST}")"
```
로그 출력 `removed_codex_tools=` → `removed_agent_tools=`.

- [x] **Step 3: 마커 제거가 옛/새 마커 모두 처리하는지 확인**

언인스톨 시 지시 파일에서 관리 블록을 제거하는 awk가 `BEGIN UECOMMANDFORGE (CODEX )?INSTRUCTIONS` 둘 다 인식하게 한다(C1 Step 5와 동일 패턴).

- [x] **Step 4: 구문 검사 + 커밋**

```bash
bash -n tools/release/uninstall.sh
git add tools/release/uninstall.sh
git commit -m "feat: uninstall supports agent flags and reads neutral+legacy manifest keys"
```

### Task C4: 설치 smoke 테스트 — HOME 격리 + per/multi agent

**Files:** `tools/test/smoke/release_package_install.sh`, `tools/test/smoke/installer_install_update_uninstall.sh`

- [x] **Step 1: 현재 격리 방식 확인**

Run: `grep -n 'codex-home\|CODEX_HOME\|\.codex\|HOME=' tools/test/smoke/release_package_install.sh tools/test/smoke/installer_install_update_uninstall.sh`
Expected: `--codex-home <temp>` 로 임시 홈을 넘기는 호출 발견.

- [x] **Step 2: HOME 격리 + --codex 로 전환**

`--codex-home "${TMP_HOME}"` 인자를 제거하고, 설치 호출을 `HOME="${TMP_HOME}" ... install-uecommandforge.sh --project ... --codex` 형태로 바꾼다. 설치 대상 경로 검증을 `${TMP_HOME}/.codex/UECommandForge/...`, 지시 파일을 `${TMP_HOME}/.codex/AGENTS.md` 로 맞춘다.

- [x] **Step 3: 다중 에이전트 케이스 추가**

`--codex --claude` 동시 설치 후 `${TMP_HOME}/.codex/AGENTS.md` 와 `${TMP_HOME}/.claude/CLAUDE.md` **양쪽**에 관리 블록(`BEGIN UECOMMANDFORGE INSTRUCTIONS`)이 주입됐는지, 각 홈에 `UECommandForge/` 트리가 생성됐는지 검증하는 assert를 추가한다.

- [x] **Step 4: 에이전트 플래그 누락 케이스 추가**

에이전트 플래그 없이 설치 호출 시 비정상 종료(exit≠0)하고 사용법 메시지를 출력하는지 검증한다.

- [x] **Step 5: 옛 마커 마이그레이션 케이스 추가**

`${TMP_HOME}/.codex/AGENTS.md` 에 옛 마커(`BEGIN/END UECOMMANDFORGE CODEX INSTRUCTIONS`) 블록을 미리 심어두고 `--codex` 재설치 후, 파일에 `BEGIN UECOMMANDFORGE INSTRUCTIONS` 가 정확히 1회, 옛 마커가 0회인지 검증한다.

- [x] **Step 6: 설치 smoke 실행**

Run: `tools/test/smoke/release_package_install.sh`
Expected: 위 모든 assert PASS.

- [x] **Step 7: 커밋**

```bash
git add tools/test/smoke/release_package_install.sh tools/test/smoke/installer_install_update_uninstall.sh
git commit -m "test: HOME-isolated installer smoke for per-agent and multi-agent flags"
```

---

## Phase D — 통합 검증 및 리뷰

### Task D1: 전체 잔여 Codex 스캔 + smoke 스위트

- [x] **Step 1: 전역 잔여 Codex 식별자 스캔**

Run:
```bash
grep -rn 'CodexReports\|--codex-home\|CODEX_HOME\|specs/codex\|codex_home\|codex_tools_path\|codex_specs_path\|codex_skill_path' . \
  | grep -v node_modules | grep -v 'sample/Saved/PluginBuild' | grep -v 'docs/memory' | grep -v 'docs/superpowers/plans/ucf-'
```
Expected: 출력 없음. (과거 세션 기록 `docs/memory/`·옛 계획 `ucf-*` 은 역사 기록이라 제외. `sample/Saved/PluginBuild` 는 빌드 산출물.)

- [x] **Step 2: 가능한 smoke 테스트 일괄 실행**

Run (엔진/프로젝트 준비된 환경에서):
```bash
for t in tools/test/smoke/*.sh; do echo "== $t =="; UECF_PROJECT_FILE=<.uproject> "$t" || break; done
```
Expected: 전부 PASS, 결과물이 `Saved/UECommandForge/Reports` 에 생성.

- [x] **Step 3: 보안 리뷰 (설치/언인스톨 변경)**

설치·언인스톨·체크섬·입력 검증 변경이 포함되므로 security-reviewer(또는 로컬 diff 리뷰)로 findings-first 리뷰를 수행하고, CRITICAL/HIGH를 검증 후 수정한다. 특히 마커 마이그레이션 awk의 중첩/주입 안전성, 다중 에이전트 순회 시 백업/심볼릭 링크 거부 로직이 각 홈에 일관 적용되는지 확인한다.

- [x] **Step 4: 문서 갱신 (doc-updater)**

`README.md` 설치 절차에 `--codex/--claude/--antigravity` 사용법을 반영하고, 코드맵/README를 한국어로 갱신한다.

- [x] **Step 5: 최종 커밋**

```bash
git add -A
git commit -m "docs: finalize multi-agent install docs and codemaps"
```

---

## Self-Review (작성자 체크 결과)

- **Spec coverage:** 섹션1(런타임 경로)=A1~A5, 섹션2(설치 프로파일)=C1~C2/C4, 섹션3(콘텐츠 중립화)=B1~B2, 섹션4(Windows)=A2·C2(래퍼/도움말)·C4(HOME 격리), 섹션5(manifest/언인스톨)=C1 Step6·C3. 모든 섹션이 태스크에 매핑됨.
- **Placeholder scan:** TODO/TBD 없음. 기계적 치환은 정확한 `sed`/`grep` 명령과 기대 출력으로 명시.
- **Type/식별자 일관성:** 변수 리네임 매핑(`CODEX_HOME`→`AGENT_HOME` 등)과 함수명(`agent_home_for`, `agent_instructions_filename_for`, `install_for_agent`, `append_agent_instructions`)을 Phase C 전반에서 동일하게 사용. 마커 문자열(`UECOMMANDFORGE INSTRUCTIONS`)과 옛-마커 정규식(`(CODEX )?`)을 C1·C3·C4에서 일치.
- **리스크 메모:** UE C++ 빌드/자동화 테스트와 smoke 테스트는 Unreal Engine + 대상 프로젝트가 있어야 실제 실행된다. 빌드 불가 환경에서는 `bash -n` 구문 검사와 `grep` 잔여 스캔까지 수행하고, 엔진 실행 검증은 별도 보고한다.
