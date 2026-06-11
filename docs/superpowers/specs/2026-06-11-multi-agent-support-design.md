# UECommandForge 범용 AI 코딩 도구 지원 설계

작성일: 2026-06-11

## 배경

UECommandForge의 자동화 규칙·설치 구조가 Codex 전용으로 결합돼 있다. Claude Code, Antigravity 등 다른 AI 코딩 에이전트에서도 동일하게 쓸 수 있도록 Codex 종속을 제거하고, 설치 시 어떤 에이전트를 대상으로 할지 플래그로 선택한다.

현재 Codex 결합은 네 개 층에 걸쳐 있다.

1. **런타임 출력 경로** — `Saved/UECommandForge/Reports` 가 플러그인 소스 약 25곳에 `TEXT("UECommandForge/Reports")` 리터럴로 흩어져 있다(단일 상수 없음). 일부 commandlet에서는 `rollback_plan` 쓰기 허용 경로(보안 경계)로도 쓰인다.
2. **설치 위치** — `CODEX_HOME` env / `--codex-home` 플래그, 기본 `~/.codex`.
3. **에이전트 지시 파일** — `~/.codex/AGENTS.md` 에 관리 블록 주입, 마커 `BEGIN/END UECOMMANDFORGE CODEX INSTRUCTIONS`.
4. **스펙/스킬 콘텐츠** — `specs/codex/unreal-automation-agents.md`, "Codex가 …" 문구, `~/.codex/...` 하드코딩 경로.

## 목표

- 설치 시 `--codex` / `--claude` / `--antigravity` 플래그로 대상 에이전트를 선택하고, 각 에이전트의 전역 폴더에 설치한다.
- 런타임 출력 경로를 도구 중립 이름으로 바꾼다.
- 지시·스펙·스킬 콘텐츠에서 Codex 고유 표현을 제거한다.

## 비목표 (YAGNI)

- 출력 경로를 env/설정으로 주입받는 런타임 추상화는 하지 않는다(고정 상수 1개로 충분).
- 설치 경로/지시 파일 **오버라이드 플래그를 제공하지 않는다**. 각 에이전트 플래그는 고정된 전역 폴더에만 설치한다.
- 기존 `CODEX_HOME` / `--codex-home` 하위호환 별칭은 유지하지 않는다(전면 제거).

---

## 섹션 1 — 런타임 출력 경로 중립화

- `Saved/UECommandForge/Reports` → **`Saved/UECommandForge/Reports`** (이미 로그·백업에 쓰이는 `Saved/UECommandForge` 아래로 그룹핑).
- 흩어진 `TEXT("UECommandForge/Reports")` 리터럴을 **단일 헬퍼/상수 1곳**으로 추출한다. `UECommandForgeEditor` 공유 헤더에 리포트 서브경로 상수(예: `kReportsSubDir = TEXT("UECommandForge/Reports")` 또는 `UECommandForge::GetReportsDir()`)를 두고, 모든 commandlet이 이를 호출한다.
- 한글 오류 메시지("rollback_plan은 Saved/UECommandForge/Reports 아래에만 쓸 수 있습니다.")를 새 경로로 갱신한다.
- 보안 가드(쓰기 허용 경계)도 동일 상수를 참조하므로 일관성이 유지된다.
- 영향: 플러그인 소스 ~25곳, C++ 테스트, smoke 테스트 24개, `.gitignore`, 문서.

## 섹션 2 — 설치 플래그 (에이전트별 전역 폴더)

설치 스크립트(`tools/release/install_local.sh` 등)에 에이전트 프로파일 테이블을 내장한다.

| 플래그 | 설치 홈 | 지시 파일 | 스킬 경로 | 관리 블록 마커 |
|---|---|---|---|---|
| `--codex` | `$HOME/.codex` | `AGENTS.md` | `<home>/skills/uecommandforge` | `UECOMMANDFORGE INSTRUCTIONS` |
| `--claude` | `$HOME/.claude` | `CLAUDE.md` | `<home>/skills/uecommandforge` | `UECOMMANDFORGE INSTRUCTIONS` |
| `--antigravity` | `$HOME/.gemini` | `GEMINI.md` | `<home>/skills/uecommandforge` | `UECOMMANDFORGE INSTRUCTIONS` |

> Antigravity(Google)는 전역 홈으로 `~/.gemini` 를 쓰고 지시 파일은 `GEMINI.md` 다(`~/.gemini/antigravity/`, `~/.gemini/config/`, `~/.gemini/GEMINI.md` 구조 확인). 플래그명은 제품명 그대로 `--antigravity` 를 유지한다.

- 에이전트 플래그가 **최소 하나** 필수다. 없으면 사용법(플래그 목록)과 함께 에러로 종료한다.
- 여러 플래그를 동시에 지정하면 각 에이전트의 전역 폴더에 **모두 순차 설치**한다(`--codex --claude` → `~/.codex` 와 `~/.claude` 양쪽). 같은 플래그 중복 지정은 무시한다.
- 설치 홈은 플래그가 정한 전역 폴더로 고정된다. 경로 오버라이드 플래그/env는 제공하지 않는다.
- 내부 변수 리네임: `CODEX_HOME`→`AGENT_HOME`, `CODEX_AGENTS_FILE`→`INSTRUCTIONS_FILE`, `CODEX_SKILL_DIR`→`SKILL_DIR`, `CODEX_ENV_FILE`/`CODEX_SHELL_ENV_FILE`→`AGENT_ENV_FILE`/`AGENT_SHELL_ENV_FILE`.

### 테스트 격리

설치 경로 오버라이드 플래그가 없으므로, smoke/통합 테스트는 `HOME` 을 임시 디렉터리로 지정해 `$HOME/.codex` 등이 임시 경로로 해석되게 한다. 기존에 `--codex-home <temp>` 로 격리하던 테스트를 `HOME=<temp>` + 에이전트 플래그 방식으로 전환한다.

### 마커 마이그레이션

마커가 `CODEX INSTRUCTIONS` → `INSTRUCTIONS` 로 바뀐다. 설치 시 **옛 마커(`UECOMMANDFORGE CODEX INSTRUCTIONS`)와 새 마커(`UECOMMANDFORGE INSTRUCTIONS`) 양쪽을 인식**해 기존 블록을 제거하고 새 블록으로 교체한다. 기존 Codex 설치본이 중복 블록을 갖지 않도록 한다.

## 섹션 3 — 지시 콘텐츠 / 스펙 중립화

- `specs/codex/unreal-automation-agents.md` → **`specs/agent/unreal-automation-agents.md`** 로 이동.
- 본문의 "Codex가 …" → **"AI 코딩 에이전트가 …"** 로 중립화. `~/.codex/UECommandForge/...` 하드코딩 경로 → `<AGENT_HOME>/UECommandForge/...` 일반 표기.
- 관리 콘텐츠 마커(`BEGIN/END UECOMMANDFORGE MANAGED CONTENT`)는 이미 중립이므로 유지.
- 저장소 루트 `AGENTS.md`: `specs/codex/...` 참조 → 새 경로, `Saved/UECommandForge/Reports/*.json` → `Saved/UECommandForge/Reports/*.json`.
- `skills/uecommandforge/SKILL.md`: "Codex skill" 등 도구 고유 표현 중립화.

## 섹션 4 — Windows 환경

Windows 설치 진입점(`install-uecommandforge.bat`, `install-uecommandforge.ps1`)은 **Git Bash를 찾아 `install-uecommandforge.sh` 에 인자를 그대로 전달하는 얇은 래퍼**다. 설치 로직은 bash 한 곳(`install-uecommandforge.sh` → `tools/release/install_local.sh`)에 모여 있다.

- **홈 해석**: Git Bash에서 `$HOME` 은 `%USERPROFILE%` 로 해석되므로 `~/.codex`, `~/.claude`, `~/.gemini` 가 `C:\Users\<user>\.codex` 등으로 올바르게 매핑된다. 별도 Windows 분기 없이 동일한 고정-홈 방식을 쓴다.
- **플래그 전달**: `.bat`/`.ps1` 는 `@InstallArgs` 를 변경 없이 넘기므로 새 `--codex`/`--claude`/`--antigravity` 플래그가 자동으로 흐른다. 두 bash 파서(`install-uecommandforge.sh` 의 passthrough case, `install_local.sh` 의 인자 case) 모두에서 새 플래그를 받고 `--codex-home` 을 제거한다.
- **런타임 `.bat` 래퍼**: `tools/ue/*.bat`(31개) 등 `Saved/UECommandForge/Reports` 를 참조하는 Windows 래퍼도 섹션 1의 `Saved/UECommandForge/Reports` 리네임 대상에 포함한다.
- **도움말 텍스트**: `install-uecommandforge.sh` / `.bat` / `.ps1` 의 사용 예시·옵션 설명에서 `--codex-home … Defaults to ~/.codex` 를 제거하고 새 에이전트 플래그를 안내한다.
- **테스트 격리**: Windows CI/Git Bash에서도 `HOME=<temp>` 로 설치 대상을 임시 경로로 돌린다. 설치 로직이 `${HOME}` 만 참조하므로 `USERPROFILE` 별도 처리 없이 격리된다.

## 섹션 5 — manifest 키 / 언인스톨러

- 설치 manifest 키 리네임: `codex_home`→`agent_home`, `codex_tools_path`→`tools_path`, `codex_specs_path`→`specs_path`, `codex_skill_path`→`skill_path`. 선택된 에이전트 이름(`codex`/`claude`/`antigravity`)도 manifest에 기록한다.
- `tools/release/uninstall.sh` 가 새 키를 읽도록 갱신한다. 옛 키(`codex_*`)도 fallback으로 인식해 기존 설치본을 제거할 수 있게 한다.
- 프로젝트 manifest(`uecommandforge-project.json`)의 `codex_*` 키도 동일하게 리네임 + fallback.

---

## 검증 기준

- `--codex` / `--claude` / `--antigravity` 각각으로 install_local.sh 실행 시 해당 전역 폴더의 올바른 지시 파일에 관리 블록이 주입된다.
- `--codex --claude` 처럼 여러 플래그를 동시에 주면 `~/.codex` 와 `~/.claude` 양쪽에 모두 설치된다.
- 에이전트 플래그 없이 실행하면 사용법과 함께 비정상 종료한다.
- 옛 마커가 박힌 AGENTS.md 재설치 시 중복 블록이 생기지 않는다.
- 플러그인 빌드 후 commandlet 실행 결과가 `Saved/UECommandForge/Reports` 에 생성된다.
- C++ 테스트 + smoke 테스트 24개가 `HOME` 격리 + 새 경로로 통과한다.
- 설치/언인스톨 변경에 대해 보안 리뷰를 병행한다(릴리즈·입력 검증·체크섬·설치 스크립트 변경 규칙).

## 리스크

- 출력 경로 리네임은 보안 가드 경계와 묶여 있어, 한 곳이라도 옛 문자열이 남으면 가드가 깨지거나 결과가 다른 폴더로 흩어진다 → 단일 상수 추출로 완화.
- 마커 변경 마이그레이션이 누락되면 기존 Codex 사용자의 AGENTS.md에 중복 블록이 남는다 → 양쪽 마커 인식으로 완화.
- `--codex-home`/`CODEX_HOME` 제거로 이를 호출하던 기존 테스트·스크립트가 깨진다 → `HOME` 격리 방식으로 일괄 전환(검증 기준에 포함).
