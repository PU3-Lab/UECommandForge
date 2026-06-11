# AGENTS.md

UECommandForge는 Unreal Engine 자동화를 commandlet과 운영체제별 wrapper로 제공하는 저장소다.

## 문서 규칙

- 문서, 계획서, PR 본문, 보고서는 한글로 작성한다.
- 코드 식별자, Unreal API/모듈명, JSON 키, CLI 플래그, 명령, 경로, 외부 로그는 원문을 유지한다.
- 커밋 제목은 Conventional Commits 영문 형식을 사용하고 본문은 한글로 작성한다.
- 새 기획·설계·작업 기록은 `docs/` 아래에 저장한다.
  - 작업 계획서: `docs/superpowers/plans/`
  - 메모리·기록: `docs/memory/`
- `README.md`, `AGENTS.md`, `SESSION_SUMMARY.md`, `specs/`, `skills/`의 문서는 기존 위치를 유지한다.
- 새 문서는 `ue_commandlet_based_llm_automation_plan.md`의 문체와 톤을 따른다.

## 작업 원칙

- 요청 범위에 필요한 부분만 수정하고, 관련 없는 기존 변경을 되돌리거나 정리하지 않는다.
- 구현 전 기존 패키지, 표준 라이브러리, 프로젝트 유틸리티를 먼저 확인한다.
- 기능 또는 버그 수정은 재현 가능한 테스트와 검증 결과를 완료 기준으로 삼는다.
- 작업 후 사용자-facing 명령이나 동작이 바뀌면 관련 README와 계획 문서를 갱신한다.

## Unreal 자동화

- Unreal Editor, Asset, Blueprint, C++ Reflection, DataTable, Config 자동화 전에는 `skills/uecommandforge/SKILL.md`를 사용한다.
- 안전 규칙은 `specs/agent/unreal-automation-agents.md`를 따른다.
- Unreal Python으로 자동화를 우회하지 않는다.
- 기존 commandlet과 wrapper를 우선하고 최신 `Saved/UECommandForge/Reports/*.json`으로 결과를 검증한다.

## 코드 리뷰

- 코드 변경 후 사용 가능한 reviewer 서브에이전트로 findings-first 리뷰를 수행한다.
- 릴리즈, 보안, 입력 검증, 체크섬, 설치 스크립트 변경은 보안 리뷰를 병행한다.
- 서브에이전트를 사용할 수 없으면 로컬 diff 리뷰로 대체하고 그 제한을 보고한다.
- CRITICAL/HIGH/MEDIUM finding은 검증 후 수정하거나 보류 사유를 명시한다.

## 작업 종료

- 사용자가 `작업 종료`라고 말하면 `SESSION_SUMMARY.md`를 현재 상태, 검증 결과, 남은 작업 기준으로 갱신한다.
- 워킹 트리에서 에이전트가 만든 변경만 확인하고 검증 후 커밋한다.
- 현재 브랜치와 원격을 확인한 뒤 푸시한다.
- 커밋 또는 푸시가 불가능하면 이유와 현재 상태를 보고한다.
