# Memory Index

모든 문서는 워킹 디렉토리(`docs/memory/`)에 저장한다. (`CLAUDE.md` 문서 저장 위치 규칙 참조)

- [No Manual Work](feedback_no_manual.md) — 수동 작업 절대 추천 금지, 항상 자동화만 제시
- [Code Review Cycle](feedback_code_review_cycle.md) — 코드 수정 후 반드시 code-reviewer 에이전트 리뷰→수정 반복, CRITICAL/HIGH 이슈 해소 후 완료 보고
- [Rule Violations Log](feedback_rule_violations.md) — 반복되는 규칙 위반 패턴 (regex 중첩괄호, 주석미제거, 타입어노테이션 누락 등) 체크리스트
- [Commit Approval Required](feedback_commit_approval.md) — 커밋/push/reset 등 git 되돌리기 어려운 작업은 반드시 사용자 승인 후 실행
- [Filename Length](feedback_filename_length.md) — 계획 파일명에 날짜 접두사 붙이지 말 것; `ucf-phase2.md` 형식 사용
- [Filename Keywords](feedback_filename_keywords.md) — 파일명 단축 시 도메인 키워드(phase 등) 탈락 금지
