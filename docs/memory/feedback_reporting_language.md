---
name: feedback_reporting_language
description: 사용자에게 보고하는 내용은 반드시 한글로 작성
metadata:
  node_type: memory
  type: feedback
  originSessionId: 2026-05-28-reporting-language
---

사용자에게 진행 상황, 리뷰 결과, 검증 결과, 완료 보고, 실패/차단 사유를 전달할 때는 반드시 한글로 작성한다.

**Why:** 사용자가 "리포팅은 꼭 한글로 규칙에 넣어"라고 명시적으로 지시했다.

**How to apply:**
1. 중간 진행 보고와 최종 답변은 기본적으로 한글로 작성한다.
2. 코드 식별자, 파일 경로, 명령어, 로그 원문, 에러 코드, 커밋 메시지는 원문을 유지할 수 있다.
3. 리뷰 결과는 심각도와 파일/라인 근거를 포함하되 설명은 한글로 작성한다.
4. 사용자가 다른 언어를 명시적으로 요청한 경우에만 해당 요청을 우선한다.
