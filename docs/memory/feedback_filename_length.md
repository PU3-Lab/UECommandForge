---
name: feedback-filename-length
description: 계획 파일명에 날짜 접두사와 긴 설명을 붙여 파일명이 과도하게 길어진 실수
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 1613c936-df93-4e43-a0cf-6263e9dad20c
---

계획 파일을 만들 때 `2026-05-25-uecommandforge-phase2.md` 같은 날짜+긴 설명 형식을 사용했다가, 사용자가 "파일명 너무 길어 짧게 줄여"라고 개입해 `ucf-phase2.md` 형식으로 변경했다.

**Why:** 파일명이 길면 터미널·에디터에서 다루기 불편하고, 날짜 접두사는 이미 git history가 관리하므로 중복 정보다.

**How to apply:** 이 프로젝트의 계획 파일 명명 규칙은 `<짧은-프로젝트-약어>-<phase|feature>.md` 형식. 날짜 접두사 불필요. 새 계획 파일 생성 전 기존 파일명 패턴을 먼저 확인한다.
