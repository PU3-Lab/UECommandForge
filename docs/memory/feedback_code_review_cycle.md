---
name: feedback_code_review_cycle
description: 코드 수정 후 반드시 리뷰→수정 사이클 반복 (사용자 명시 지침)
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 897a1210-8b3d-469b-8864-3028ea6e197e
---

코드를 작성하거나 수정한 직후에는 리뷰 전용 에이전트를 우선 사용해 자동 리뷰를 수행하고, 관련 스킬을 보조 체크리스트로 병행한다. CRITICAL/HIGH 이슈를 모두 수정한 후 다시 테스트를 통과시켜야 한다. 수정 없이 "완료"로 보고하면 안 된다.

**Why:** 사용자가 "코드 수정후에는 리뷰 수정 반복 꼭해"라고 명시적으로 지시. Sprint 3-F 구현 직후 리뷰에서 CRITICAL(add-task MISSING_STATE 미검증) 및 HIGH(dangling transition, orphaned parent) 이슈가 발견됨.

**How to apply:**
1. 코드 파일을 변경하면 → 리뷰 전용 에이전트를 먼저 사용한다.
2. C++/Unreal 변경은 `cpp-reviewer`, 일반 변경은 `code-reviewer` 또는 `reviewer`, 보안/입력/경로/권한 영향은 `security-reviewer`를 우선한다.
3. 리뷰 스킬은 보조 체크리스트로 병행한다. 기본 코드 품질은 `coding-standards`, 보안/입력/경로/권한 영향은 `security-review`, 테스트 전략은 `tdd-workflow` 또는 `verification-loop`를 사용한다.
4. 리뷰 전용 에이전트를 사용할 수 없는 런타임에서는 Codex가 직접 diff 기반 구조화 리뷰를 수행하고, 사용하지 못한 사유를 보고한다.
5. 리뷰 결과는 심각도, 파일/라인 근거, 수정 필요 여부를 한글로 보고한다.
6. CRITICAL 이슈 → 즉시 수정 필수
7. HIGH 이슈 → 해당 PR/커밋 전 수정 필수
8. 수정 후 관련 단위 테스트, automation, smoke 테스트를 재실행하여 녹색 확인
9. 이 사이클을 CRITICAL/HIGH 이슈가 0개가 될 때까지 반복
