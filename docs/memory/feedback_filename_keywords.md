---
name: feedback-filename-keywords
description: 파일명을 줄일 때 의미 있는 키워드(phase 등)를 탈락시킨 실수
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 1613c936-df93-4e43-a0cf-6263e9dad20c
---

파일명을 짧게 줄이면서 `ucf-p2.md` 처럼 `phase`를 `p`로 축약했다가, 사용자가 "phase는 넣어"라고 개입해 `ucf-phase2.md`로 재변경했다.

**Why:** `p2`는 의미가 불명확하다. `phase2`는 스프린트 단위라는 맥락이 바로 전달된다. 줄이더라도 의미가 손실되면 안 된다.

**How to apply:** 파일명 단축 시 숫자·약어로 대체할 수 있는 것(날짜, 긴 설명)만 제거한다. 도메인 키워드(`phase`, `spec`, `builder` 등)는 그대로 유지한다.
