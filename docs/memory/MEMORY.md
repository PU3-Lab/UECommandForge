# Memory Index

모든 문서는 워킹 디렉토리(`docs/memory/`)에 저장한다. (`CLAUDE.md` 문서 저장 위치 규칙 참조)

- [No Manual Work](feedback_no_manual.md) — 수동 작업 절대 추천 금지, 항상 자동화만 제시
- [Code Review Cycle](feedback_code_review_cycle.md) — 코드 수정 후 사용 가능한 자동 리뷰 방식으로 리뷰→수정 반복, CRITICAL/HIGH 이슈 해소 후 완료 보고
- [Rule Violations Log](feedback_rule_violations.md) — 반복되는 규칙 위반 패턴 (regex 중첩괄호, 주석미제거, 타입어노테이션 누락 등) 체크리스트
- [Filename Length](feedback_filename_length.md) — 계획 파일명에 날짜 접두사 붙이지 말 것; `ucf-phase2.md` 형식 사용
- [Filename Keywords](feedback_filename_keywords.md) — 파일명 단축 시 도메인 키워드(phase 등) 탈락 금지
- [Code Review Agent](feedback_code_review_agent.md) — 코드 품질 리뷰는 리뷰 전용 에이전트를 우선 사용하고, 관련 스킬을 보조 체크리스트로 병행
- [Reporting Language](feedback_reporting_language.md) — 사용자에게 진행/리뷰/검증/완료 보고 시 반드시 한글로 작성
- [Post-Work Documentation](feedback_post_work_documentation.md) — 작업 완료 후 관련 문서와 진행 상태를 항상 업데이트
- [Post-Commit Next Task](feedback_post_commit_next_task.md) — 커밋 직후 커밋 결과와 다음 작업을 반드시 함께 보고
- [Session 2026-05-25](session-2026-05-25.md) — Phase 1 완료 검증, DefaultInput.ini gitignore 처리, 체크박스 완료 커밋
- [Session 2026-05-26](session-2026-05-26.md) — run_automation_tests.sh 수정, Phase 3 Blueprint Builder·Commandlet·Shell Wrapper 3종 구현
