# CLAUDE.md

이 저장소에서 작업할 때 Claude(및 다른 LLM 에이전트)가 따라야 할 규칙.

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

## Git 규칙

- **커밋은 반드시 사용자 승인 후 실행한다.**
  - `git commit` 전에 커밋 메시지와 대상 파일 목록을 사용자에게 먼저 보여주고, 명시적 승인을 받은 뒤에만 실행한다.
  - `git push`, `git reset`, `git rebase` 등 되돌리기 어려운 작업도 동일하게 승인 후 실행한다.

## 문서 저장 위치 규칙

- **모든 문서는 워킹 디렉토리(`docs/`) 안에 저장한다.**
  - 작업 계획서: `docs/superpowers/plans/`
  - 메모리·기록 문서: `docs/memory/`
  - 기획서·설계서: `docs/`
- `~/.claude/` 등 전역 경로에 프로젝트 관련 문서를 저장하지 않는다.
- 새 문서 유형이 필요하면 `docs/` 하위에 적절한 서브디렉토리를 만든다.

## 참고

- 기획서: `ue_commandlet_based_llm_automation_plan.md`
- 작업 계획서: `docs/superpowers/plans/`
- 메모리·기록: `docs/memory/`
