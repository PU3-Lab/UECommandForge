---
name: feedback_code_review_cycle
description: 코드 수정 후 반드시 리뷰→수정 사이클 반복 (사용자 명시 지침)
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 897a1210-8b3d-469b-8864-3028ea6e197e
---

코드를 작성하거나 수정한 직후에는 반드시 code-reviewer 에이전트로 리뷰를 돌리고, CRITICAL/HIGH 이슈를 모두 수정한 후 다시 테스트를 통과시켜야 한다. 수정 없이 "완료"로 보고하면 안 된다.

**Why:** 사용자가 "코드 수정후에는 리뷰 수정 반복 꼭해"라고 명시적으로 지시. Sprint 3-F 구현 직후 리뷰에서 CRITICAL(add-task MISSING_STATE 미검증) 및 HIGH(dangling transition, orphaned parent) 이슈가 발견됨.

**How to apply:**
1. 코드 파일을 Edit/Write로 변경하면 → 즉시 code-reviewer 에이전트 실행
2. CRITICAL 이슈 → 즉시 수정 필수
3. HIGH 이슈 → 해당 PR/커밋 전 수정 필수
4. 수정 후 단위 테스트 + smoke 테스트 전부 재실행하여 녹색 확인
5. 이 사이클을 이슈가 0개가 될 때까지 반복
