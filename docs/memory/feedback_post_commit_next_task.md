---
name: feedback_post_commit_next_task
description: 커밋 직후 다음 작업을 반드시 함께 보고
metadata:
  node_type: memory
  type: feedback
  originSessionId: 2026-05-28-post-commit-next-task
---

커밋을 완료한 뒤에는 커밋 해시와 워킹트리 상태만 보고하고 끝내지 말고, 사용자가 요청한 다음 작업까지 반드시 함께 보고한다.

**Incident:** 2026-05-28에 사용자가 "커밋 후에 다음 작업 보고"라고 지시했지만, `935f1c4 fix: harden asset snapshot output` 커밋 후 커밋 결과만 보고하고 다음 작업을 누락했다.

**Why:** 커밋 완료 보고와 다음 작업 보고가 분리되면 사용자가 다시 확인해야 하고, sprint 진행 흐름이 끊긴다.

**How to apply:**
1. `git commit` 실행 후 최종 보고에는 항상 `커밋`, `상태`, `다음 작업`을 포함한다.
2. 다음 작업은 sprint/phase 문서의 미완료 체크박스에서 가장 앞선 항목으로 판단한다.
3. 다음 작업이 모호하면 "다음 작업 후보"가 아니라 현재 문서 기준의 단일 권장 작업을 먼저 말한다.
4. 사용자가 "다음 작업 말하라", "커밋 후 다음 작업 보고"를 반복 지적한 이력이 있으므로 커밋 완료 응답 전 반드시 이 문서를 체크한다.
