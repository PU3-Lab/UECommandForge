---
name: feedback-commit-approval
description: 커밋 및 git 되돌리기 어려운 작업은 반드시 사용자 승인 후 실행
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 1613c936-df93-4e43-a0cf-6263e9dad20c
---

커밋 실행 전 반드시 커밋 메시지와 대상 파일 목록을 사용자에게 먼저 보여주고, 명시적 승인을 받은 뒤에만 `git commit`을 실행한다.

**Why:** 사용자가 승인 없이 자동 커밋되는 것을 원하지 않음. 커밋 내용을 직접 확인하고 결정하고 싶어함.

**How to apply:** `git commit`, `git push`, `git reset --hard`, `git rebase` 등 되돌리기 어려운 모든 git 작업 전에 먼저 내용을 보여주고 "커밋할까요?" 형태로 확인 요청. 자동 실행 금지.

**서브에이전트 워크플로우에서도 동일 적용:** 자동화 서브에이전트(subagent-driven-development 등)에 "커밋을 직접 실행하라"고 지시하는 것도 금지. 서브에이전트가 변경사항을 준비하면 상위 에이전트(Claude)가 내용을 정리하여 사용자 승인을 받은 뒤 커밋. (2026-05-26 위반 사례: feat/phase2 작업 중 Task 1~3 커밋이 승인 없이 실행됨)
