# AGENTS.md

이 저장소에서 작업할 때 LLM 에이전트가 따라야 할 규칙.

## 문서 작성 규칙

- **모든 문서는 한글로 작성한다.**
  - 기획서, 작업 계획서(`docs/superpowers/plans/*.md`), README, PR 본문, 커밋 메시지 본문, 코드 주석 중 설명에 해당하는 부분, 보고/리포트 문서 모두 한글이 기본.
  - 단, 다음은 예외로 영문/원문 유지:
    - 코드 식별자 (클래스/함수/변수/파일명) — 영문 그대로.
    - Unreal API/모듈명, JSON 키, CLI 플래그, 셸 명령, 경로 — 영문 그대로.
    - 외부 인용/에러 메시지/로그 — 원문 그대로 인용한 뒤 필요 시 한글 해설을 덧붙인다.
    - 커밋 메시지 **제목**(첫 줄)은 Conventional Commits 영문(`feat:`, `fix:` 등). 본문은 한글.
- 표/리스트의 헤더와 설명도 한글. 기술 용어는 한글(영문) 병기 가능. 예: `커맨드렛(Commandlet)`.
- 새 문서를 만들 때는 기존 문서(`ue_commandlet_based_llm_automation_plan.md`)의 문체와 톤을 따른다.

## 문서 저장 위치 규칙

- **모든 문서는 워킹 디렉토리(`docs/`) 안에 저장한다.**
  - 작업 계획서: `docs/superpowers/plans/`
  - 메모리·기록 문서: `docs/memory/`
  - 기획서·설계서: `docs/`
- `~/.claude/` 등 전역 경로에 프로젝트 관련 문서를 저장하지 않는다.
- 새 문서 유형이 필요하면 `docs/` 하위에 적절한 서브디렉토리를 만든다.

## Unreal 자동화 작업 규칙

- Unreal Editor/에셋/Blueprint 자동화 작업을 수행하기 전에 `specs/codex/unreal-automation-agents.md`를 읽고 그대로 따른다.
- 설치 스크립트는 위 파일의 내용을 `~/.codex/AGENTS.md`의 UECommandForge managed block에 삽입한다. 지침 본문을 shell heredoc에 중복 작성하지 않는다.

### UECommandForge 실행 속도 규칙

- 반복 생성 작업은 단건 wrapper보다 batch wrapper를 우선 사용한다.
  - Blueprint 여러 개: `tools/ue/create_blueprint_batch.*`
  - Blueprint 생성과 기본 컴포넌트/기본값 적용: batch spec의 `components`와 defaults 기능을 우선 검토한다.
- `CreateBlueprint` 또는 `CreateBlueprintBatch` Result JSON에 `compile_status: ok`, `asset_on_disk: true`, `asset_in_registry: true`가 있으면 같은 턴에서 `CompileBlueprints`를 반복 실행하지 않는다.
- C++ 새 클래스 생성 후 Blueprint 생성은 반드시 UHT/build 또는 reflection 검증 뒤 새 Unreal 프로세스에서 실행한다.
- Result 확인은 전체 로그보다 최신 `Saved/CodexReports/*.json`의 `ok`, `errors`, `issues`, `validation`을 우선한다.

### UECommandForge 생성 작업 결정 트리

1. Blueprint만 여러 개 만들면 `create_blueprint_batch.*`를 사용한다.
2. C++ 클래스만 여러 개 만들면 `generate_cpp_class_batch.*`를 사용한다.
3. 새 C++ 부모 클래스 기반 Blueprint를 만들면 다음 순서를 지킨다.
   - `generate_cpp_class_batch.*` 또는 `generate_cpp_class.*`
   - Unreal build/UHT 또는 `validate_cpp_reflection.*`
   - `create_blueprint_batch.*` 또는 `create_blueprint.*`
4. `CreateBlueprint*` Result JSON이 compile/save/registry 검증을 통과하면 같은 턴의 `compile_blueprints.*`는 생략한다.
5. 실패하면 임시 Python 우회 없이 Result JSON, project log, crash report를 먼저 확인한다.



## 코드 리뷰 규칙

- **Codex CLI에서 서브에이전트 도구가 제공되면 `reviewer` 서브에이전트로 코드 리뷰를 수행한다.**
  - 이 저장소에서는 코드 변경 후 리뷰 단계가 발생하면 사용자가 서브에이전트 기반 코드 리뷰를 명시적으로 요청한 것으로 간주한다.
  - `spawn_agent` 같은 서브에이전트 실행 도구가 제공되는 환경에서는 로컬 수동 리뷰보다 `reviewer` 또는 `code-reviewer` 서브에이전트 리뷰를 우선한다.
  - 릴리즈 패키징, 보안, 입력 검증, 체크섬, 설치 스크립트 변경은 `security_reviewer`도 함께 사용한다.
  - 구현 완료 후 findings를 먼저 보고하고, CRITICAL/HIGH/MEDIUM은 기술적으로 검증한 뒤 수정하거나 명확한 사유로 보류한다.
  - 서브에이전트가 금지되거나 사용할 수 없는 환경이면 로컬 코드 리뷰로 대체하고 그 제한을 보고한다.

## 작업 종료 규칙

- 사용자가 `작업 종료`라고 말하면 세션 마무리 절차를 수행한다.
  - `SESSION_SUMMARY.md`를 현재 작업 상태, 검증 결과, 남은 작업 기준으로 갱신한다.
  - 워킹 트리를 확인하고 필요한 변경을 커밋한다.
  - 커밋 후 원격 브랜치에 푸시한다.
  - 커밋 또는 푸시가 불가능하면 이유와 현재 상태를 보고한다.

## 참고

- 기획서: `ue_commandlet_based_llm_automation_plan.md`
- 작업 계획서: `docs/superpowers/plans/`
- 메모리·기록: `docs/memory/`
