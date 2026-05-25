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
